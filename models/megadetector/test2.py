import xir

def tname(t):
    return getattr(t, "get_name", lambda: getattr(t, "name", "?"))()

def tshape(t):
    return getattr(t, "get_shape", lambda: getattr(t, "dims", lambda: "?") )()

g = xir.Graph.deserialize("./megadetector.xmodel")
root = g.get_root_subgraph()

print("== ROOT GRAPH OUTPUT TENSORS ==")
# Try both spellings across versions
get_outs = getattr(root, "get_output_tensors",
                   lambda: getattr(root, "outputs", lambda: [] )() )
outs = get_outs()
for i, t in enumerate(outs):
    name = tname(t)
    shape = tshape(t)
    print(f"OUT[{i}]: {name}  shape={shape}")
