import xir, vart, sys

def tensor_name(t):
    # try several spellings
    for k in ("get_name", "name"):
        if hasattr(t, k):
            v = getattr(t, k)
            return v() if callable(v) else v
    return "?"

def tensor_shape(t):
    # different versions expose different attrs
    for k in ("get_shape", "shape", "dims", "get_dims"):
        if hasattr(t, k):
            v = getattr(t, k)
            return v() if callable(v) else v
    # last resort: element count only
    return f"<unknown, elems={getattr(t,'get_element_num', lambda: '?')()}>"

def dump_xmodel_io(xmodel_path):
    g = xir.Graph.deserialize(xmodel_path)
    dpu_sgs = [s for s in g.get_root_subgraph().toposort_child_subgraph()
               if s.has_attr("device") and s.get_attr("device") == "DPU"]
    for idx, sg in enumerate(dpu_sgs):
        print(f"\n=== DPU subgraph #{idx}: {sg.get_name()} ===")
        r = vart.Runner.create_runner(sg, "run")
        ins = r.get_input_tensors()
        outs = r.get_output_tensors()
        for i,t in enumerate(ins):
            print(f" IN[{i}]: {tensor_name(t)}  shape={tensor_shape(t)}")
        for i,t in enumerate(outs):
            print(f"OUT[{i}]: {tensor_name(t)}  shape={tensor_shape(t)}")

dump_xmodel_io("./megadetector.xmodel")
