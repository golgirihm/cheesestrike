"""Tests for infra/template.yaml, as far as it can be checked without AWS:
that it is a valid template, and that it agrees with the code it sets up.

    python -m unittest discover -s infra
"""

import pathlib
import sys
import tempfile
import unittest

import cfnlint.api
from cfnlint.decode import cfn_yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "infra" / "template.yaml"
sys.path.insert(0, str(ROOT / "signaling" / "cloud"))

import deploy  # noqa: E402
import dynamo_store  # noqa: E402


def plain(value):
    """A template value as ordinary dicts, lists and strings."""
    if isinstance(value, dict):
        return {str(key): plain(item) for key, item in value.items()}
    if isinstance(value, list):
        return [plain(item) for item in value]
    return str(value) if isinstance(value, str) else value


class TemplateTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.resources = plain(cfn_yaml.load(str(TEMPLATE)))["Resources"]

    def test_the_template_is_valid(self):
        problems = cfnlint.api.lint(TEMPLATE.read_text(encoding="utf-8"))
        self.assertEqual([str(problem) for problem in problems], [])

    def test_the_table_is_laid_out_as_the_code_expects(self):
        table = self.resources["Table"]["Properties"]
        for part in ("AttributeDefinitions", "KeySchema", "GlobalSecondaryIndexes", "BillingMode"):
            self.assertEqual(table[part], dynamo_store.SCHEMA[part], part)
        self.assertEqual(table["TimeToLiveSpecification"], dynamo_store.TIME_TO_LIVE)

    def test_the_function_runs_the_handler_that_exists(self):
        function = self.resources["Function"]["Properties"]
        module, name = function["Handler"].split(".")
        self.assertEqual((TEMPLATE.parent / function["CodeUri"]).resolve(), deploy.LAMBDA, "where the deploy builds it")
        with tempfile.TemporaryDirectory() as built:
            folder = pathlib.Path(built)
            deploy.build_function(folder)
            self.assertIn(f"def {name}(event, context)", (folder / f"{module}.py").read_text(encoding="utf-8"))

    def test_every_kind_of_websocket_event_reaches_the_function(self):
        routes = {r["Properties"]["RouteKey"] for r in self.resources.values() if r["Type"] == "AWS::ApiGatewayV2::Route"}
        self.assertEqual(routes, {"$connect", "$disconnect", "$default"})

    def test_the_session_list_can_be_read_by_the_games_page(self):
        url = self.resources["Function"]["Properties"]["FunctionUrlConfig"]
        self.assertEqual(url["AuthType"], "NONE")
        self.assertEqual(url["Cors"]["AllowOrigins"], ["*"])


if __name__ == "__main__":
    unittest.main()
