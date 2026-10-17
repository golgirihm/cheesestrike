"""Signaling server for CheeseStrike on AWS: the rules in signaling/core.py,
run by one Lambda function that is called once for each thing that happens
and remembers nothing in between. Sessions are kept in DynamoDB
(signaling/cloud/dynamo_store.py). infra/template.yaml sets all of it up, and
docs/backend.md describes it.

The function is called from two places, told apart by the event it is given:

  - API Gateway's WebSocket API, which holds the connections of hosts and of
    players joining, calls it when one connects, sends a message or
    disconnects. Replies and relayed messages go back through API Gateway.
  - Its own function URL, for plain HTTP requests:

        GET /sessions          {"sessions": [{"code": "ABCD", "players": 3, "settings": {...}}]}
        GET /sessions/ABCD     {"open": true}    whether that session's host is connected

    The function URL adds the headers that let the game's page read these.

API Gateway limits the size and rate of messages (the stage's settings in the
template), so there are no such limits here.
"""

import json
import os

import boto3

import core
from dynamo_store import DynamoStore

# Made once and kept while AWS keeps this copy of the function around, so
# that most calls don't pay to set them up.
_signaling = None
_senders = {}  # API Gateway endpoint -> client that sends to its connections


def handler(event, context):
    request = event.get("requestContext", {})
    if "connectionId" in request:
        return _websocket(event, request)
    return _http(event, request)


def signaling():
    global _signaling
    if _signaling is None:
        table = boto3.resource("dynamodb").Table(os.environ["TABLE_NAME"])
        _signaling = core.Signaling(DynamoStore(table))
    return _signaling


def _websocket(event, request):
    connection = request["connectionId"]
    kind = request.get("eventType")
    if kind == "CONNECT":
        address = request.get("identity", {}).get("sourceIp", "?")
        if not signaling().connect(connection, address):
            return {"statusCode": 429, "body": "Too many connections from this address."}
        return {"statusCode": 200}
    if kind == "DISCONNECT":
        out = signaling().disconnect(connection)
    else:
        out = signaling().message(connection, event.get("body") or "")
    _deliver(out, f"https://{request['domainName']}/{request['stage']}")
    return {"statusCode": 200}


def _deliver(out, endpoint):
    """Sends what the rules said to send, in order, through API Gateway."""
    if not out:
        return
    sender = _senders.get(endpoint)
    if sender is None:
        sender = _senders[endpoint] = boto3.client("apigatewaymanagementapi", endpoint_url=endpoint)
    for connection, msg in out:
        try:
            if msg is None:
                sender.delete_connection(ConnectionId=connection)
            else:
                sender.post_to_connection(ConnectionId=connection, Data=json.dumps(msg).encode())
        except sender.exceptions.GoneException:
            pass  # They have already disconnected.


def _http(event, request):
    method = request.get("http", {}).get("method", "GET")
    path = event.get("rawPath", "/").rstrip("/")
    if method != "GET":
        return _answer(405, {"error": "Only GET is allowed."})
    if path == "/sessions":
        return _answer(200, {"sessions": signaling().list_sessions()})
    if path.startswith("/sessions/"):
        return _answer(200, {"open": signaling().session_open(path.removeprefix("/sessions/"))})
    return _answer(404, {"error": "Not found."})


def _answer(status, body):
    return {
        "statusCode": status,
        "headers": {"Content-Type": "application/json", "Cache-Control": "no-store"},
        "body": json.dumps(body),
    }
