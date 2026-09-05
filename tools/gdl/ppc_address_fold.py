"""Proof for one allocator/peephole address-forwarding idiom, not a rewriter.

The only accepted pair is contiguous non-record/non-OE add; addi; lwz,
with a temporary consumed by the load in one stream and a persistent base
in the other. Both execute exactly one word load at the same effective
address and finish with every GPR equal for arbitrary incoming registers.
CR/XER/LR/CTR/FPRs and memory are untouched. Arithmetic is modulo 2**32.

This models normal instruction completion, not intermediate register
snapshots observable by a debugger or hardware exception handler. The ELF
caller separately proves entry boundaries, relocations, identity outside
the window and complete input/target/output hashes. No liveness assumption
or target-dependent premise enters this algebraic proof.
"""

from collections import Counter

MASK32 = (1 << 32) - 1


def signed16(word: int) -> int:
    value = word & 0xffff
    return value - 0x10000 if value & 0x8000 else value


def fields(word: int) -> tuple[int, int, int]:
    return (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31


def decode(words: tuple[int, ...]) -> str:
    if len(words) != 3 or any(type(w) is not int or not 0 <= w <= MASK32
                              for w in words):
        raise ValueError("address fold requires exactly three 32-bit words")
    first, advance, load = words
    if first >> 26 != 31 or first & 0x7ff != 532:
        raise ValueError("address fold requires non-record non-OE add")
    if advance >> 26 != 14 or load >> 26 != 32:
        raise ValueError("address fold requires addi then lwz")
    if fields(advance)[1] == 0 or fields(load)[1] == 0:
        raise ValueError("address fold refuses RA-zero addressing")
    if any(fields(word)[0] in (1, 2, 13) for word in words):
        raise ValueError("address fold refuses SP/TOC/SDA writes")
    temporary = fields(first)[0]
    pointer, advance_base, _ = fields(advance)
    loaded, load_base, _ = fields(load)
    if not signed16(advance) or advance_base != temporary:
        raise ValueError("address fold requires a nonzero dependent base advance")
    if pointer != temporary and load_base == temporary and loaded == temporary:
        return "folded"
    if pointer == temporary and load_base == pointer and loaded != pointer:
        return "unfolded"
    raise ValueError("address fold is outside the temporary/base/load idiom")


def _add(left: tuple, right: tuple) -> tuple:
    terms = Counter(dict(left))
    terms.update(dict(right))
    return tuple(sorted((key, coefficient & MASK32)
                        for key, coefficient in terms.items()
                        if coefficient & MASK32))


def state(words: tuple[int, ...]) -> tuple:
    decode(words)
    registers = [((f"r{r}", 1),) for r in range(32)]
    first, advance, load = words
    d, a, b = fields(first)
    registers[d] = _add(registers[a], registers[b])
    d, a, _ = fields(advance)
    registers[d] = _add(registers[a], (("constant", signed16(advance)),))
    d, a, _ = fields(load)
    address = _add(registers[a], (("constant", signed16(load)),))
    # There is one load, last, and no stores. Equal addresses mean an equal
    # value from the same initial memory; the token is not assumed constant.
    registers[d] = (("loaded_word", 1),)
    return address, tuple(registers)


def prove_address_fold(ours: tuple[int, ...], target: tuple[int, ...]) -> str:
    our_form, target_form = decode(ours), decode(target)
    if our_form == target_form:
        raise ValueError("address fold requires opposite folded/unfolded forms")
    if fields(ours[0])[1:] != fields(target[0])[1:]:
        raise ValueError("address fold may not exchange the add inputs")
    if (fields(ours[1])[0] != fields(target[1])[0]
            or fields(ours[2])[0] != fields(target[2])[0]):
        raise ValueError("address fold requires identical pointer/load destinations")
    delta = signed16(ours[1])
    if delta != signed16(target[1]):
        raise ValueError("address fold base advances differ")
    folded, unfolded = (ours, target) if our_form == "folded" else (target, ours)
    if signed16(folded[2]) != signed16(unfolded[2]) + delta:
        raise ValueError("address fold signed displacements do not sum")
    our_address, our_registers = state(ours)
    target_address, target_registers = state(target)
    if our_address != target_address:
        raise ValueError("address fold load effective addresses differ")
    if our_registers != target_registers:
        raise ValueError("address fold final register values differ")
    return "ours_" + our_form
