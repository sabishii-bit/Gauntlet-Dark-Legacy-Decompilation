"""HV lane run-28: permutation-window + order SEARCH driving the shipped stage.

ha_close.build_rule proves a rule but takes its permutation windows as GIVEN.
HB's searcher (hb_tierb.py) was untracked lane scratch and is gone.  This
rebuilds the search with the two run-27 corrections applied:

  * SCREEN 3 FIX (claim.law.HB_two-censuses-...v1): test the UNWIDENED cluster
    FIRST and widen only on failure.  Widening before the control-op test
    rejects functions whose true window is control-free but which sit next to
    a branch -- and that is where MWCC schedule differences live.
  * COPY->COPY ARROW (same law): ha_close.classify already carries it
    (`uncond` / `uncond_rc`); this module must not re-derive a classifier
    that drops it.

and two disciplines made mechanical:

  * ENUMERATE EVERY SHAPE-CONSISTENT ORDER AND SELECT BY PROOF
    (claim.law.WF_enumerate-...v1): never return the first shape-consistent
    bijection.  Every candidate order is carried into the full proof.
  * STEP-0 MEMBERSHIP (claim.law.C1_...v1): a PRE window's order must be legal
    in OUR colouring standing alone -- check_permutation_dependences is run as
    a MEMBERSHIP test before any rule is built.

Nothing here simulates a stage it could call.
"""
import itertools
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Repo root, located by landmark so this file runs unchanged from the
# lane scratch directory AND from tools/gdl/composed_census after
# promotion (discipline 17: a promoted script must actually run).
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    parent = os.path.dirname(ROOT)
    if parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf            # noqa: E402
import ha_close as ha            # noqa: E402
from cn_analyze import our_object, target_object, load  # noqa: E402

MAX_WINDOW_ATOMS = 20
MAX_ORDERS_PER_WINDOW = 64
MAX_COMBOS = 48
# The multi-window permutation schema (claim.law.WF_multi-window-permutation-
# schema.20260901.v1) makes several windows ordinary, so this is a SEARCH cost
# bound, not a schema bound.  Raised from 3 to 8 after the first sweep pass
# refused 29 candidates on it -- a bound of one's own making is a coverage gap
# like any other and has to be measured, not assumed harmless.
MAX_CLUSTERS = int(os.environ.get("HV_MAX_CLUSTERS", "8"))


def _mask(word):
    try:
        return wf.register_slot_mask(word)
    except ValueError:
        return None


def unrecolourable(word):
    """lmw/stmw: register_slot_mask is 0, so their fields can never be fixed."""
    return (word >> 26) in (46, 47)


def compat(ow, tw):
    """May OUR atom `ow` legally land on the target slot holding `tw`?

    Absorbable afterwards means: identical, a pure register-field difference
    (copy_register_fields' job), or a copy-form site the combined stage serves.
    This is exactly ha_close.classify's vocabulary, reused rather than
    re-derived -- a classifier and the rule it sizes must share their arrows.
    """
    if ow == tw:
        return True
    what, _info = ha.classify(ow, tw)
    return what in ("regfield", "form")


POOL_OURS = __import__("re").compile(r"^@+\d+$")
POOL_TGT = __import__("re").compile(r"^lbl_[0-9A-Fa-f]{8}$")


def word_relocs(relmap):
    """Relocations keyed by the WORD they belong to, never by raw offset.

    claim.law.HV_emb-sda21-relocation-offset-differs-...v1: an EMB_SDA21 entry
    is recorded at instruction+2 in MWCC output and at instruction+0 in the
    dtk-extracted target.  Keying by raw offset makes our side look
    RELOCATION-FREE at exactly those words, which silently deletes every
    candidate for the target slot that does carry one.  (This canary caught
    that: dcs::update_chinfo destination +0x70 had zero candidate sources.)
    """
    out = {}
    for off, ident in relmap.items():
        out.setdefault(off & ~3, []).append((off & 3, ident))
    return {w: tuple(sorted(v)) for w, v in out.items()}


def _pool(sym, ours):
    return bool((POOL_OURS if ours else POOL_TGT).match(sym or ""))


def symbols_bind(ours_ident, tgt_ident, corr):
    """Bind a relocated atom to its target slot by (type, symbol).

    claim.law.HV_permute-payload-check-does-not-bind-a-relocation-to-its-atom:
    the shipped guard proves CONSERVATION, not binding, so binding is the
    deriver's obligation.  Compiler pool labels are spelled `@NNN` in our
    output and `lbl_XXXXXXXX` in the target, so they are bound through the
    whole-function correspondence `corr` derived outside the moving windows,
    and every non-pool symbol is bound by exact name.
    """
    if (ours_ident is None) != (tgt_ident is None):
        return False
    if ours_ident is None:
        return True
    if len(ours_ident) != len(tgt_ident):
        return False
    for (obyte, (otype, osym)), (tbyte, (ttype, tsym)) in zip(ours_ident,
                                                              tgt_ident):
        if otype != ttype:
            return False
        if osym == tsym:
            continue
        if osym in corr:
            if corr[osym] != tsym:
                return False
            continue
        if not (_pool(osym, True) and _pool(tsym, False)):
            return False
    return True


def correspondence(owr, twr, windows):
    """our-symbol -> target-symbol, read off words NOT being moved."""
    seen = {}
    for w, ours_ident in owr.items():
        if any(lo <= w < hi for lo, hi in windows):
            continue
        tgt_ident = twr.get(w)
        if tgt_ident is None or len(tgt_ident) != len(ours_ident):
            continue
        for (_ob, (ot, osym)), (_tb, (tt, tsym)) in zip(ours_ident, tgt_ident):
            if ot != tt:
                continue
            seen.setdefault(osym, set()).add(tsym)
    return {s: next(iter(v)) for s, v in seen.items() if len(v) == 1}


def window_ok(words_ours, words_tgt, lo, hi):
    """Control-free in BOTH streams.  That is the ONLY structural bar.

    An earlier draft also rejected any window CONTAINING an lmw/stmw, copying
    HA's ha_widen screen.  That is a coarse guard of exactly the kind
    discipline 14 is about, and it refused a provable function: DdrawGlowText-
    MLines' epilogue window is `lmw / lwz / lfd` and its lmw only MOVES -- it
    never needs a register field changed, because `compat` already demands an
    lmw land on a byte-identical lmw (register_slot_mask raises for opcode
    46/47, so regfield_only is False and decode_copy_form is None, leaving
    equality as the only way through).  The fine check subsumes the coarse one;
    the coarse one cost HB a screen-3-style false refusal on the same function
    from the other side.
    """
    for off in range(lo, hi, 4):
        for w in (wf._u32(words_ours, off), wf._u32(words_tgt, off)):
            if wf._is_control_instruction(w):
                return False
    return True


def enumerate_orders(ours, tgt, orel, trel, lo, hi, limit=MAX_ORDERS_PER_WINDOW):
    """Every bijection whose atoms are shape- AND relocation-consistent.

    Returns a list of `order` lists in apply_patch's convention:
    order[d] = s means destination slot d receives OUR source atom s.
    """
    n = (hi - lo) // 4
    if n < 2 or n > MAX_WINDOW_ATOMS:
        return []
    owr, twr = word_relocs(orel), word_relocs(trel)
    corr = correspondence(owr, twr, [(lo, hi)])
    our_words = [wf._u32(ours, lo + 4 * i) for i in range(n)]
    tgt_words = [wf._u32(tgt, lo + 4 * i) for i in range(n)]
    our_rel = [owr.get(lo + 4 * i) for i in range(n)]
    tgt_rel = [twr.get(lo + 4 * i) for i in range(n)]

    # candidates[d] = sources that may fill destination d
    candidates = []
    for d in range(n):
        row = []
        for s in range(n):
            # relocations ride their atom; BIND them (not merely conserve)
            if not symbols_bind(our_rel[s], tgt_rel[d], corr):
                continue
            if compat(our_words[s], tgt_words[d]):
                row.append(s)
        if not row:
            return []
        candidates.append(row)

    out = []
    used = [False] * n
    order = [0] * n

    def rec(d):
        if len(out) >= limit:
            return
        if d == n:
            out.append(list(order))
            return
        for s in candidates[d]:
            if used[s]:
                continue
            used[s] = True
            order[d] = s
            rec(d + 1)
            used[s] = False

    rec(0)
    # The identity is not a permutation; drop it (it changes nothing).
    return [o for o in out if o != list(range(n))]


def candidate_windows(ours, tgt, lo, hi, widen=6):
    """UNWIDENED FIRST, then symmetric and asymmetric widenings."""
    n = len(ours)
    seen = []
    for extra in range(0, widen + 1):
        for left in range(0, extra + 1):
            right = extra - left
            a, b = lo - 4 * left, hi + 4 * right
            if a < 0 or b > n:
                continue
            if (b - a) // 4 > MAX_WINDOW_ATOMS:
                continue
            if (a, b) in seen:
                continue
            seen.append((a, b))
    return seen


def clusters(offsets, gap=16):
    out = []
    for off in sorted(offsets):
        if out and off - out[-1][1] <= gap:
            out[-1][1] = off
        else:
            out.append([off, off])
    return [(a, b + 4) for a, b in out]


def unabsorbed(ours, tgt):
    return [off for off in range(0, len(ours), 4)
            if ha.classify(wf._u32(ours, off), wf._u32(tgt, off))[0] == "other"]


def differing(ours, tgt):
    """Every offset whose word differs, absorbed or not."""
    return [off for off in range(0, len(ours), 4)
            if wf._u32(ours, off) != wf._u32(tgt, off)]


def widen_across_absorbed(cl, diffs, limit=MAX_WINDOW_ATOMS):
    """Extend each cluster across ADJACENT DIFFERING words it absorbed.

    claim.law.WF_per-offset-absorption-decided-before-window-selection-hides-
    the-true-permutation-window: absorption and movement are the two competing
    explanations of the SAME word, and deciding absorption first at a fixed
    offset forecloses movement. A word that differs only in register slots
    (mechanism a) or decodes as a copy-form arrow in place (mechanism b) is
    dropped from the unabsorbed set, the cluster window selection is built from
    shrinks, and the correct window is never offered to the prover.

    Absorption is a HYPOTHESIS about a word, not an observation, so this
    reopens it — but only across words that actually DIFFER, and only up to
    the window-atom bound. A blanket absorption-off pass is useless here and
    was measured so: game/movie/movieplayer::fn_800D8BCC differs in 122 of
    215 words, which merges into clusters far past MAX_WINDOW_ATOMS and
    refuses immediately, offering nothing.
    """
    diffs = set(diffs)
    out = []
    for lo, hi in cl:
        while (lo - 4) in diffs and (hi - (lo - 4)) // 4 <= limit:
            lo -= 4
        while hi in diffs and ((hi + 4) - lo) // 4 <= limit:
            hi += 4
        out.append((lo, hi))
    # Widening can make two neighbours overlap; merge so the windows stay
    # pairwise disjoint, which the combination step below requires.
    merged = []
    for lo, hi in sorted(out):
        if merged and lo <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], hi))
        else:
            merged.append((lo, hi))
    return [(lo, hi) for lo, hi in merged
            if (hi - lo) // 4 <= limit]


def spread_by_window(sols, cap=8):
    """Up to `cap` solutions, ROUND-ROBIN across distinct windows.

    The plain `sols[:cap]` truncation re-imposes exactly the narrowness the
    absorption retry exists to remove: solutions come out narrowest-window
    first, so a cap of 8 hands the prover eight orders from the SAME narrow
    window and never reaches the wider one. Measured on
    game/movie/movieplayer::fn_800D8BCC — the retry offered the widened
    cluster and the wider window's orders were still cut off by the cap.
    """
    by_window = {}
    for sol in sols:
        by_window.setdefault((sol["lo"], sol["hi"]), []).append(sol)
    out = []
    rank = 0
    while len(out) < cap:
        added = False
        for window in by_window:
            if rank < len(by_window[window]):
                out.append(by_window[window][rank])
                added = True
                if len(out) >= cap:
                    break
        if not added:
            break
        rank += 1
    return out


def solve_cluster(ours, tgt, orel, trel, clo, chi, placement,
                  narrowest_only=True):
    """Windows+orders that make this cluster absorbable, membership-screened.

    For a PRE window the order must be legal in OUR colouring standing alone
    (step-0 membership, claim.law.C1).  For a POST window the stage runs in the
    target colouring after a proven recolor, and webfrank audits it there; we
    only screen shape, relocation-freedom and control-freedom here and let
    apply_patch adjudicate.

    ``narrowest_only`` is the other half of the absorption law. Stopping at
    the first width that yields ANY order is a second way the true window
    goes unoffered: a narrow window can enumerate orders that no rule can
    prove while a wider one proves immediately, and the break means the
    prover never sees the wider one. The retry pass clears it.
    """
    found = []
    for lo, hi in candidate_windows(ours, tgt, clo, chi):
        if not window_ok(ours, tgt, lo, hi):
            continue
        if placement == "post":
            # the shipped stage refuses ANY relocation in the window
            if any(lo <= o < hi for o in list(orel) + list(trel)):
                continue
        orders = enumerate_orders(ours, tgt, orel, trel, lo, hi)
        for order in orders:
            if placement == "pre":
                region = bytes(ours[lo:hi])
                try:
                    wf.check_permutation_dependences(region, order)
                except ValueError:
                    continue
                except TypeError:
                    # Shipped-guard defect: check_permutation_dependences
                    # sorts its `broken` list of (atom, resource) keys, whose
                    # resource is a str for "mem"/"lr"/"cr" and a tuple for
                    # ('g',N)/("stack",N).  Two broken chains on the SAME atom
                    # compare str against tuple and raise from the ERROR PATH.
                    # The refusal is correct; only its message dies -- so
                    # treat it as the refusal it is instead of letting it
                    # abort the sweep (it aborted 5 candidates before this).
                    continue
            found.append({"lo": lo, "hi": hi, "order": order})
        if found and narrowest_only:
            # UNWIDENED (or narrowest) wins: stop at the first width that works
            break
    return found


def search(unit, fn, verbose=False):
    """Return (rule, note).  Proof is always through the shipped apply_patch."""
    op, kind = our_object(unit)
    tp = target_object(unit)
    if not (os.path.exists(op) and os.path.exists(tp)):
        return None, "no object"
    _od, _os, _oy, ours, orel, ojt = load(op, fn)
    _td, _ts, _ty, tgt, trel, _tj = load(tp, fn)
    if len(ours) != len(tgt):
        return None, f"size mismatch ({len(ours)//4} vs {len(tgt)//4})"
    if ours == tgt:
        return None, "already identical in the raw body"

    # Tier A: no permutation needed at all.
    rest = unabsorbed(ours, tgt)
    if not rest:
        rule, resid, note = ha.build_rule(unit, fn)
        return (rule, f"TIER-A {note}") if rule else (None, f"TIER-A {note}")

    cl = clusters(rest)
    if verbose:
        print(f"  {len(rest)} unabsorbed words in {len(cl)} cluster(s): "
              + ", ".join(f"+0x{a:x}..+0x{b:x}" for a, b in cl))
    if len(cl) > MAX_CLUSTERS:
        return None, f"{len(cl)} unabsorbed clusters (over search bound)"

    # TWO PASSES (run-35 item 12). The first is the narrow search as it has
    # always run. If it refuses, the refusal is not yet a fact about the
    # function: per-offset absorption may have eaten the true window's
    # boundary words before window selection ever saw them, and the
    # narrowest-width break may have foreclosed a wider window that proves.
    # claim.law.WF_per-offset-absorption-decided-before-window-selection-
    # hides-the-true-permutation-window is explicit that absorption is a
    # HYPOTHESIS about a word, and its stated procedure is to widen and
    # re-run BEFORE recording the refusal. Retrying costs one extra search
    # only on functions that already failed.
    diffs = differing(ours, tgt)
    passes = [("narrow", cl, True)]
    reopened = widen_across_absorbed(cl, diffs)
    if reopened != cl:
        passes.append(("absorption-reopened", reopened, False))

    last_note = "no proved composition"
    for label, cluster_set, narrowest_only in passes:
        if verbose and label != "narrow":
            print("  RETRY with per-offset absorption REOPENED: "
                  + ", ".join(f"+0x{a:x}..+0x{b:x}" for a, b in cluster_set))
        for placement in ("pre", "post"):
            per_cluster = []
            ok = True
            for clo, chi in cluster_set:
                sols = solve_cluster(ours, tgt, orel, trel, clo, chi,
                                     placement,
                                     narrowest_only=narrowest_only)
                if not sols:
                    ok = False
                    break
                per_cluster.append(sols[:8] if narrowest_only
                                   else spread_by_window(sols, 8))
            if not ok:
                continue
            combos = list(itertools.islice(itertools.product(*per_cluster),
                                           MAX_COMBOS))
            last = "no combination proved"
            for combo in combos:
                wins = sorted(combo, key=lambda w: w["lo"])
                if any(wins[i]["hi"] > wins[i + 1]["lo"]
                       for i in range(len(wins) - 1)):
                    continue      # windows must be pairwise disjoint
                kw = {"pre": wins} if placement == "pre" else {"post": wins}
                try:
                    rule, resid, note = ha.build_rule(unit, fn, **kw)
                except Exception as exc:                  # noqa: BLE001
                    last = f"{type(exc).__name__}: {exc}"
                    continue
                if rule:
                    return rule, (f"CLOSES via {placement}-permutation"
                                  f" ({kind}, {label} windows)")
                last = note
            if verbose:
                print(f"  {label}/{placement}: {last}")
            last_note = last
    # Say WHICH search refused, so the note cannot be read as "absorption was
    # never reopened" — the whole point of the law is that the narrow
    # refusal was being recorded as a property of the function.
    if len(passes) == 1:
        return None, ("no proved composition (no absorbed word adjacent to"
                      " any cluster, so there was no wider window to reopen)")
    return None, f"no proved composition after absorption retry: {last_note}"


if __name__ == "__main__":
    import json
    u, f = sys.argv[1], sys.argv[2]
    r, note = search(u, f, verbose=True)
    print(f"{u}::{f}: {note}")
    if r:
        print(json.dumps(r, indent=1))
