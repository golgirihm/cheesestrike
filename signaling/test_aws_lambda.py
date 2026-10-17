"""Tests for the signaling server as AWS runs it: the events API Gateway and
the function URL hand the function, and what it does with them. DynamoDB is
moto's imitation, and API Gateway's way of sending to a connection is a
stand-in here. Nothing reaches AWS.

    python -m unittest discover -s signaling
"""

import json
import os
import unittest

import boto3
from moto import mock_aws

import pathlib
import sys

# The two ways of running the rules, each in a folder of its own.
HERE = pathlib.Path(__file__).resolve().parent
sys.path[:0] = [str(HERE / "local"), str(HERE / "cloud")]

import aws_lambda  # noqa: E402
import core  # noqa: E402
import dynamo_store  # noqa: E402

DOMAIN = "abc123.execute-api.us-east-2.amazonaws.com"


class Gone(Exception):
    pass


class Sender:
    """Stands in for API Gateway's client for sending to connections."""

    class exceptions:
        GoneException = Gone

    def __init__(self):
        self.sent = {}  # connection -> messages
        self.closed = []
        self.gone = set()

    def post_to_connection(self, ConnectionId, Data):
        if ConnectionId in self.gone:
            raise Gone()
        self.sent.setdefault(ConnectionId, []).append(json.loads(Data))

    def delete_connection(self, ConnectionId):
        if ConnectionId in self.gone:
            raise Gone()
        self.closed.append(ConnectionId)


class LambdaTest(unittest.TestCase):
    def setUp(self):
        # Made-up credentials, so that nothing can reach a real account even
        # if the imitation were somehow bypassed.
        for name in ("AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY", "AWS_SESSION_TOKEN"):
            os.environ[name] = "testing"
        os.environ["AWS_DEFAULT_REGION"] = "us-east-2"
        os.environ["TABLE_NAME"] = "signaling"
        mock = mock_aws()
        mock.start()
        self.addCleanup(mock.stop)
        dynamo_store.create_table(boto3.resource("dynamodb"), "signaling")
        self.sender = Sender()
        aws_lambda._signaling = None
        aws_lambda._senders = {f"https://{DOMAIN}/prod": self.sender}

    def websocket(self, kind, connection, body=None, address="203.0.113.5"):
        event = {
            "requestContext": {
                "routeKey": {"CONNECT": "$connect", "DISCONNECT": "$disconnect"}.get(kind, "$default"),
                "eventType": kind,
                "connectionId": connection,
                "domainName": DOMAIN,
                "stage": "prod",
                "identity": {"sourceIp": address},
            },
        }
        if body is not None:
            event["body"] = body if isinstance(body, str) else json.dumps(body)
        return aws_lambda.handler(event, None)

    def get(self, path, method="GET"):
        answer = aws_lambda.handler({"rawPath": path, "requestContext": {"routeKey": "$default", "http": {"method": method, "path": path}}}, None)
        return answer["statusCode"], json.loads(answer["body"])

    def host(self, connection="host=", **options):
        self.websocket("CONNECT", connection)
        self.websocket("MESSAGE", connection, {"type": "host", **options})
        return self.sender.sent[connection][-1]["code"]

    def test_hosting_joining_and_relaying_through_api_gateway(self):
        code = self.host()
        self.assertEqual(self.websocket("CONNECT", "joiner=")["statusCode"], 200)
        self.websocket("MESSAGE", "joiner=", {"type": "join", "code": code})
        peer = self.sender.sent["joiner="][-1]["id"]
        self.assertEqual(self.sender.sent["host="][-1], {"type": "peer_joined", "id": peer})

        self.websocket("MESSAGE", "joiner=", {"type": "offer", "sdp": "the offer"})
        self.assertEqual(self.sender.sent["host="][-1], {"type": "offer", "sdp": "the offer", "from": peer})
        self.websocket("MESSAGE", "host=", {"type": "answer", "to": peer, "sdp": "the answer"})
        self.assertEqual(self.sender.sent["joiner="][-1], {"type": "answer", "sdp": "the answer", "from": core.HOST_ID})

    def test_the_session_list_and_whether_one_is_open_are_read_over_http(self):
        self.assertEqual(self.get("/sessions"), (200, {"sessions": []}))
        code = self.host(settings={"map": "city"})
        self.assertEqual(self.get("/sessions"), (200, {"sessions": [{"code": code, "players": 1, "settings": core.DEFAULT_SETTINGS | {"map": "city"}}]}))
        self.assertEqual(self.get(f"/sessions/{code}"), (200, {"open": True}))
        self.assertEqual(self.get(f"/sessions/{code.lower()}/"), (200, {"open": True}))
        self.assertEqual(self.get("/sessions/ZZZZ"), (200, {"open": False}))

    def test_other_http_requests_are_refused(self):
        self.assertEqual(self.get("/")[0], 404)
        self.assertEqual(self.get("/anything/else")[0], 404)
        self.assertEqual(self.get("/sessions", method="POST")[0], 405)

    def test_a_host_disconnecting_closes_the_session_and_those_still_joining(self):
        code = self.host()
        self.websocket("CONNECT", "joiner=")
        self.websocket("MESSAGE", "joiner=", {"type": "join", "code": code})
        self.websocket("DISCONNECT", "host=")
        self.assertEqual(self.sender.sent["joiner="][-1], {"type": "closed"})
        self.assertEqual(self.sender.closed, ["joiner="])
        self.assertEqual(self.get(f"/sessions/{code}"), (200, {"open": False}))
        self.assertEqual(self.get("/sessions"), (200, {"sessions": []}))

    def test_sending_to_someone_who_has_gone_is_not_an_error(self):
        code = self.host()
        self.websocket("CONNECT", "joiner=")
        self.websocket("MESSAGE", "joiner=", {"type": "join", "code": code})
        self.sender.gone.add("joiner=")
        self.assertEqual(self.websocket("DISCONNECT", "host=")["statusCode"], 200)

    def test_too_many_connections_from_one_address_are_refused_at_the_door(self):
        was = core.MAX_CONNECTIONS_PER_ADDRESS
        core.MAX_CONNECTIONS_PER_ADDRESS = 2
        self.addCleanup(setattr, core, "MAX_CONNECTIONS_PER_ADDRESS", was)
        self.assertEqual(self.websocket("CONNECT", "a=")["statusCode"], 200)
        self.assertEqual(self.websocket("CONNECT", "b=")["statusCode"], 200)
        self.assertEqual(self.websocket("CONNECT", "c=")["statusCode"], 429)
        self.assertEqual(self.websocket("CONNECT", "d=", address="203.0.113.6")["statusCode"], 200)
        self.websocket("DISCONNECT", "a=")
        self.assertEqual(self.websocket("CONNECT", "c=")["statusCode"], 200)

    def test_a_message_with_no_body_or_nonsense_is_answered_not_crashed_on(self):
        self.websocket("CONNECT", "someone=")
        for body in (None, "", "not json", "[]"):
            self.assertEqual(self.websocket("MESSAGE", "someone=", body)["statusCode"], 200)
            self.assertEqual(self.sender.sent["someone="][-1], {"type": "error", "message": "Malformed message."})

    def test_a_host_takes_its_session_back_on_a_new_connection(self):
        self.websocket("CONNECT", "host=")
        self.websocket("MESSAGE", "host=", {"type": "host"})
        hosted = self.sender.sent["host="][-1]
        self.websocket("DISCONNECT", "host=")
        self.websocket("CONNECT", "again=")
        self.websocket("MESSAGE", "again=", {"type": "resume", "code": hosted["code"], "token": hosted["token"]})
        self.assertEqual(self.sender.sent["again="][-1], {"type": "resumed", "code": hosted["code"]})
        self.assertEqual(self.get(f"/sessions/{hosted['code']}"), (200, {"open": True}))


if __name__ == "__main__":
    unittest.main()
