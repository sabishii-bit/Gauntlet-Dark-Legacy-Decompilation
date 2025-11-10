import re
import hashlib


def emptyHash():
    return hashlib.sha1()


def hashString(string):
    return hashlib.sha1(string.encode())


"""
(?:^|(?:^\s+))
Included before the pragma search in order to match the beginning of a line
or the beginning + any whitespace. Ignore the result of this capture.
Used to enforce that there are no comments or anything else before the pragma.

(#pragma\sGLOBAL_ASM\((.*?)\))
1st Match = entire pragma line
2nd Match = pragma arguments
"""
pragmaRegex = r"(?:^|(?:^\s+))(#pragma\sGLOBAL_ASM\((.*?)\))"


def getPragmaMatches(fileText):
    matches = re.findall(pragmaRegex, fileText, flags=re.DOTALL | re.MULTILINE)
    return matches


def getPragmaArgs(argString):
    args = argString.split(",")
    args = map(lambda s: s.split("\"")[1], args)
    return list(args)


# --- Label and ASM helpers ---

labelRegex = r"^\s*([A-Za-z_\.][A-Za-z0-9_\.]*):"


def getLabels(fileText):
    """Return all labels (functions and globals) from ASM text."""
    matches = []
    for line in fileText.splitlines():
        m = re.match(labelRegex, line)
        if m:
            matches.append(m.group(1) + ":")
    return matches


def getAsmSymbols(fileText):
    """Return ALL symbol labels (no filtering of lbl_)."""
    return getLabels(fileText)


def getAsmBlock(fileText, label):
    """
    Collect lines after `label` up to (but not including) the next label or EOF.
    Does not rely on blank lines to terminate.
    """
    data = []
    found = False
    for line in fileText.splitlines():
        m = re.match(labelRegex, line)
        if m:
            current = m.group(1) + ":"
            if not found and current == label:
                found = True
                continue
            elif found:
                break  # stop at next label
        if found:
            data.append(line.strip())
    return data


def filterBlockCode(block):
    return list(filter(lambda x: "/*" in x, block))


def codeLineToBytes(line):
    d = line.split()
    b = "0x" + "".join(d[3:7])
    return b


def blockToBytes(block):
    code = filterBlockCode(block)
    return list(map(codeLineToBytes, code))


def bytesToString(byteLine):
    return "opword " + byteLine


funcTemplate = """extern "C" {
asm void {name}() {
nofralloc
{data}
}}"""

funcTemplateNoSig = """nofralloc
{data}"""


def writeCode(source, funcName, codeBytes, isGlobal, noSig):
    source += "\n"
    if noSig:
        t = funcTemplateNoSig
    else:
        t = funcTemplate.replace("{name}", funcName)
    bs = list(map(bytesToString, codeBytes))
    t = t.replace("{data}", "\n".join(bs))
    if not isGlobal:
        t = t.replace("asm void", "asm static void")
    source += t + "\n"
    return source


def error(text):
    print("Error during #pragma processing:")
    print(text)
    exit(69)
