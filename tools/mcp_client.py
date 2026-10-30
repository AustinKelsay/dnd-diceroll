"""Small Streamable HTTP client for the pinned Unreal MCP implementation."""
import json
import urllib.error
import urllib.request


def decode_response(response, request_id):
    if "text/event-stream" not in response.headers.get("Content-Type", ""):
        result = json.load(response)
        if result.get("id") != request_id:
            raise RuntimeError("MCP response ID mismatch")
        return result
    event_data = []
    for raw in response:
        line = raw.decode().rstrip("\r\n")
        if line.startswith("data:"):
            event_data.append(line[5:].lstrip())
        elif not line and event_data:
            result = json.loads("\n".join(event_data))
            event_data = []
            if result.get("id") == request_id:
                return result
    raise RuntimeError("MCP stream ended without the requested response")


class MCP:
    def __init__(self, url):
        self.url = url
        self.session = None
        self.sequence = 0
        self.protocol = "2025-11-25"
        self.schemas = {}

    def request(self, method, params=None, notification=False):
        self.sequence += 1
        message = {"jsonrpc": "2.0", "method": method}
        if not notification:
            message["id"] = self.sequence
        if params is not None:
            message["params"] = params
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session:
            headers.update({"Mcp-Session-Id": self.session, "MCP-Protocol-Version": self.protocol})
        try:
            req = urllib.request.Request(self.url, json.dumps(message).encode(), headers)
            with urllib.request.urlopen(req, timeout=180) as response:
                self.session = response.headers.get("Mcp-Session-Id", self.session)
                if notification or response.status == 202:
                    return None
                result = decode_response(response, self.sequence)
        except urllib.error.HTTPError as exc:
            raise RuntimeError(f"MCP HTTP {exc.code}: {exc.read(8192).decode(errors='replace')}") from exc
        if "error" in result:
            raise RuntimeError(f"MCP error: {result['error']}")
        value = result.get("result")
        if isinstance(value, dict) and value.get("isError"):
            raise RuntimeError(f"MCP tool failed: {value}")
        return value

    @staticmethod
    def text_value(result):
        for block in result.get("content", []):
            if block.get("type") == "text":
                return json.loads(block["text"])
        raise RuntimeError("MCP result contains no JSON text")

    def describe(self, toolset):
        if toolset not in self.schemas:
            self.schemas[toolset] = self.text_value(self.request("tools/call", {
                "name": "describe_toolset", "arguments": {"toolset_name": toolset},
            }))
        return self.schemas[toolset]

    def call(self, toolset, tool, arguments=None):
        schema = self.describe(toolset)
        matches = [t for t in schema["tools"] if t["name"].rsplit(".", 1)[-1] == tool]
        if len(matches) != 1:
            raise RuntimeError(f"Tool unavailable in running engine: {toolset}.{tool}")
        arguments = arguments or {}
        missing = set(matches[0]["inputSchema"].get("required", [])) - arguments.keys()
        if missing:
            raise ValueError(f"Missing tool arguments: {sorted(missing)}")
        result = self.request("tools/call", {"name": "call_tool", "arguments": {
            "toolset_name": toolset, "tool_name": tool, "arguments": arguments,
        }})
        value = self.text_value(result)
        if "returnValue" not in value:
            raise RuntimeError(f"Unexpected tool result from {tool}: {value}")
        return value["returnValue"]

    def __enter__(self):
        try:
            self.info = self.request("initialize", {"protocolVersion": self.protocol, "capabilities": {},
                "clientInfo": {"name": "unreal-starter-dev", "version": "0.2.0"}})
            self.protocol = self.info["protocolVersion"]
            self.request("notifications/initialized", notification=True)
            # Catalog discovery precedes toolset descriptions and dispatch.
            self.catalog = self.request("tools/call", {"name": "list_toolsets", "arguments": {}})
            return self
        except Exception:
            self.__exit__()
            raise

    def __exit__(self, *unused):
        if self.session:
            try:
                req = urllib.request.Request(self.url, method="DELETE", headers={
                    "Mcp-Session-Id": self.session, "MCP-Protocol-Version": self.protocol})
                with urllib.request.urlopen(req, timeout=10):
                    pass
            except OSError:
                pass
