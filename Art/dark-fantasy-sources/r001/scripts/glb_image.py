import json, struct, sys
data = open(sys.argv[1], "rb").read()
jlen = struct.unpack_from("<I", data, 12)[0]
gltf = json.loads(data[20:20 + jlen]); binoff = 20 + jlen + 8
for i, img in enumerate(gltf.get("images", [])):
    view = gltf["bufferViews"][img["bufferView"]]
    start = binoff + view.get("byteOffset", 0)
    open(sys.argv[2], "wb").write(data[start:start + view["byteLength"]]); break
uv = gltf["meshes"][0]["primitives"][0]["attributes"].get("TEXCOORD_0")
acc = gltf["accessors"][uv]; print("uv min/max", acc.get("min"), acc.get("max"))
