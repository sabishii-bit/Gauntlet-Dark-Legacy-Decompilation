import glob
import json

pats = ["*DC_copy-form*", "*LZ_webfrank*", "*C1_permute*", "*HV_single*"]
for pat in pats:
    for p in glob.glob("memory_graph/records/**/" + pat + ".json",
                       recursive=True):
        d = json.load(open(p, encoding="utf-8"))
        print(d.get("id"), "\n   subject:", d.get("subject"),
              "| predicate:", d.get("predicate"))
