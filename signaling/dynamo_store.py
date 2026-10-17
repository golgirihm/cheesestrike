"""Where the signaling rules keep sessions and connections on AWS: one
DynamoDB table. It has the same methods as signaling/memory_store.py, which
says what a session and a connection's record hold, and
signaling/test_stores.py holds both to the same behavior.

Nothing here assumes one caller at a time. Many copies of the rules run at
once on AWS, so whatever has to happen exactly once, such as handing out a
session code or deciding who a session's host is, is a single conditional
write: DynamoDB applies it only if the condition still holds, and tells the
loser.

The table's layout is `SCHEMA`, which infra/template.yaml has to match
(infra/test_template.py checks). Each item has a string key, `id`:

    s#ABCD          a session
    c#<connection>  a connection

and three attributes that exist only to be looked things up by:

    expires   when the item stops counting, in seconds since 1970. DynamoDB
              deletes expired items by itself, but in its own time, so every
              read here checks it too.
    owner     the address a connection came from or a session is hosted
              from, for counting how much one address is using. A session
              has it only while its host is connected.
    listed    "1" on a session that belongs in the public list: one that
              isn't private and whose host is connected.
"""

import decimal

from boto3.dynamodb.conditions import Attr, Key
from botocore.exceptions import ClientError

SCHEMA = {
    "AttributeDefinitions": [
        {"AttributeName": "id", "AttributeType": "S"},
        {"AttributeName": "owner", "AttributeType": "S"},
        {"AttributeName": "listed", "AttributeType": "S"},
        {"AttributeName": "created", "AttributeType": "N"},
    ],
    "KeySchema": [{"AttributeName": "id", "KeyType": "HASH"}],
    "GlobalSecondaryIndexes": [
        {
            "IndexName": "by_owner",
            "KeySchema": [{"AttributeName": "owner", "KeyType": "HASH"}, {"AttributeName": "id", "KeyType": "RANGE"}],
            "Projection": {"ProjectionType": "INCLUDE", "NonKeyAttributes": ["expires"]},
        },
        {
            "IndexName": "listed",
            "KeySchema": [{"AttributeName": "listed", "KeyType": "HASH"}, {"AttributeName": "created", "KeyType": "RANGE"}],
            "Projection": {"ProjectionType": "ALL"},
        },
    ],
    "BillingMode": "PAY_PER_REQUEST",
}
TIME_TO_LIVE = {"AttributeName": "expires", "Enabled": True}

SESSION_FIELDS = ("code", "token", "host", "address", "settings", "private", "players", "peers", "created", "expires")


class DynamoStore:
    def __init__(self, table):
        """`table` is a boto3 DynamoDB Table resource laid out as `SCHEMA`."""
        self.table = table

    def add_connection(self, connection, address, expires):
        self.table.put_item(Item=_to_db({"id": _connection_id(connection), "address": address, "owner": address, "expires": expires}))

    def connection(self, connection, now):
        item = self.table.get_item(Key={"id": _connection_id(connection)}, ConsistentRead=True).get("Item")
        if item is None or item["expires"] <= now:
            return None
        return _from_db({key: item[key] for key in ("address", "expires", "role", "code", "peer") if key in item})

    def set_connection(self, connection, role, code, peer):
        self._update(
            _connection_id(connection),
            "SET #role = :role, #code = :code, #peer = :peer",
            "attribute_exists(#id)",
            {"#id": "id", "#role": "role", "#code": "code", "#peer": "peer"},
            {":role": role, ":code": code, ":peer": peer},
        )

    def remove_connection(self, connection):
        item = self.table.delete_item(Key={"id": _connection_id(connection)}, ReturnValues="ALL_OLD").get("Attributes")
        return _from_db(item) if item is not None else None

    def count_connections(self, address, now):
        return self._count_owned(address, "c#", now)

    def count_sessions(self, address, now):
        """How many sessions `address` is hosting now. One whose host is away
        has no owner, so it isn't counted."""
        return self._count_owned(address, "s#", now)

    def create_session(self, session):
        """Keeps a new session, unless its code is taken. Says whether it did."""
        item = dict(session, id=_session_id(session["code"]))
        if item.get("host") is None:
            item.pop("host", None)
        else:
            item["owner"] = item["address"]
            if not item["private"]:
                item["listed"] = "1"
        try:
            self.table.put_item(
                Item=_to_db(item),
                # An expired session may still be in the table; its code is free.
                ConditionExpression="attribute_not_exists(#id) OR #expires <= :created",
                ExpressionAttributeNames={"#id": "id", "#expires": "expires"},
                ExpressionAttributeValues=_to_db({":created": session["created"]}),
            )
        except ClientError as error:
            if _condition_failed(error):
                return False
            raise
        return True

    def session(self, code, now):
        item = self.table.get_item(Key={"id": _session_id(code)}, ConsistentRead=True).get("Item")
        return _session(item) if item is not None and item["expires"] > now else None

    def update_session(self, code, host, fields):
        """Changes a session, but only for the connection that is its host."""
        names = {f"#{name}": name for name in fields} | {"#host": "host"}
        values = {f":{name}": value for name, value in fields.items()} | {":host_now": host}
        assignments = ", ".join(f"#{name} = :{name}" for name in fields)
        return self._update(_session_id(code), f"SET {assignments}", "#host = :host_now", names, values)

    def release_host(self, code, host, expires):
        """Marks a session's host as away, if `host` is still its host."""
        return self._update(
            _session_id(code),
            "SET #expires = :expires REMOVE #host, #owner, #listed",
            "#host = :host_now",
            {"#expires": "expires", "#host": "host", "#owner": "owner", "#listed": "listed"},
            {":expires": expires, ":host_now": host},
        )

    def claim_host(self, code, token, host, address, expires):
        """Makes `host` a session's host, for whoever holds its token."""
        session = self.table.get_item(Key={"id": _session_id(code)}, ConsistentRead=True).get("Item")
        if session is None:
            return False
        expression = "SET #host = :host, #address = :address, #owner = :address, #expires = :expires"
        names = {"#host": "host", "#address": "address", "#owner": "owner", "#expires": "expires", "#token": "token"}
        values = {":host": host, ":address": address, ":expires": expires, ":token_given": token}
        if not session["private"]:
            expression += ", #listed = :listed"
            names["#listed"] = "listed"
            values[":listed"] = "1"
        return self._update(_session_id(code), expression, "#token = :token_given", names, values)

    def set_peer(self, code, peer, connection):
        names = {"#id": "id", "#peers": "peers", "#peer": str(peer)}
        self._update(_session_id(code), "SET #peers.#peer = :connection", "attribute_exists(#id)", names, {":connection": connection})

    def remove_peer(self, code, peer):
        names = {"#id": "id", "#peers": "peers", "#peer": str(peer)}
        self._update(_session_id(code), "REMOVE #peers.#peer", "attribute_exists(#id)", names, {})

    def public_sessions(self, limit, now):
        """Up to `limit` open sessions that aren't private, newest first."""
        listed = []
        query = {
            "IndexName": "listed",
            "KeyConditionExpression": Key("listed").eq("1"),
            "FilterExpression": Attr("expires").gt(_to_db(now)),
            "ScanIndexForward": False,
        }
        while len(listed) < limit:
            page = self.table.query(**query)
            listed += [_session(item) for item in page["Items"]]
            if "LastEvaluatedKey" not in page:
                break
            query["ExclusiveStartKey"] = page["LastEvaluatedKey"]
        return listed[:limit]

    def _count_owned(self, address, prefix, now):
        count = 0
        query = {
            "IndexName": "by_owner",
            "KeyConditionExpression": Key("owner").eq(address) & Key("id").begins_with(prefix),
            "FilterExpression": Attr("expires").gt(_to_db(now)),
            "Select": "COUNT",
        }
        while True:
            page = self.table.query(**query)
            count += page["Count"]
            if "LastEvaluatedKey" not in page:
                return count
            query["ExclusiveStartKey"] = page["LastEvaluatedKey"]

    def _update(self, key, expression, condition, names, values):
        """Applies an update if `condition` holds; says whether it did.
        Attributes are always named through `names`, since many of ours are
        words DynamoDB reserves."""
        arguments = {
            "Key": {"id": key},
            "UpdateExpression": expression,
            "ConditionExpression": condition,
            "ExpressionAttributeNames": names,
        }
        if values:
            arguments["ExpressionAttributeValues"] = _to_db(values)
        try:
            self.table.update_item(**arguments)
        except ClientError as error:
            if _condition_failed(error):
                return False
            raise
        return True


def create_table(dynamodb, name):
    """Makes a table laid out as `SCHEMA`: for tests, and for trying things
    against DynamoDB Local. The real one is made by infra/template.yaml."""
    table = dynamodb.create_table(TableName=name, **SCHEMA)
    table.wait_until_exists()
    return table


def _session_id(code):
    return f"s#{code}"


def _connection_id(connection):
    return f"c#{connection}"


def _session(item):
    session = _from_db({key: item[key] for key in SESSION_FIELDS if key in item})
    session.setdefault("host", None)
    return session


def _condition_failed(error):
    return error.response["Error"]["Code"] == "ConditionalCheckFailedException"


def _to_db(value):
    """`value` with its floats as the Decimals DynamoDB wants."""
    if isinstance(value, float):
        return decimal.Decimal(str(value))
    if isinstance(value, dict):
        return {key: _to_db(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_to_db(item) for item in value]
    return value


def _from_db(value):
    """`value` with DynamoDB's Decimals as ordinary numbers."""
    if isinstance(value, decimal.Decimal):
        return int(value) if value == value.to_integral_value() else float(value)
    if isinstance(value, dict):
        return {key: _from_db(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_from_db(item) for item in value]
    return value
