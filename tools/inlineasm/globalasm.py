import argparse
from pathlib import Path
from cache import createCacheFolder, getFileCache, saveFileCache
from helpers import *

info = """globalasm.py:

Inserts assembly directly into CPP source.
Specified by #pragma GLOBAL_ASM(assemblyFilePath, functionName)
"""

parser = argparse.ArgumentParser(description=info)
parser.add_argument("file", help="The source file to process")
parser.add_argument("outfile", help="The file to output to")
parser.add_argument(
    "-s",
    "--scope",
    help="Set function scope equal to scope in assembly",
    action="store_true",
)


def run():
    args = parser.parse_args()
    sourcePath = Path(args.file)
    sourceText = open(sourcePath).read()
    outPath = Path(args.outfile)
    matches = getPragmaMatches(sourceText)

    # Prepare cache
    createCacheFolder()
    fileCache = getFileCache(sourcePath)

    # Preload ASM files
    asmFileDictionary = {}
    for match in matches:
        pragmaArgs = getPragmaArgs(match[1])
        asmPath = Path(pragmaArgs[0])
        func = pragmaArgs[1]
        key = str(asmPath)

        if key not in asmFileDictionary:
            fileText = open(asmPath).read()
            symbols = set(getAsmSymbols(fileText))
            asmFileDictionary[key] = {"text": fileText, "symbols": symbols}

        if func + ":" not in asmFileDictionary[key]["symbols"]:
            error(f"function or label: '{func}' not in {key}")

    # Process pragmas
    for match in matches:
        replacePragmaText = match[0]
        pragmaArgs = getPragmaArgs(match[1])

        funcHash = emptyHash()
        funcHash.update(pragmaArgs[0].encode())
        funcHash.update(pragmaArgs[1].encode())
        hashKey = funcHash.hexdigest()

        if hashKey in fileCache:
            sourceText = sourceText.replace(replacePragmaText, fileCache[hashKey])
            continue

        asmPath = Path(pragmaArgs[0])
        key = str(asmPath)
        asmFileText = asmFileDictionary[key]["text"]

        funcToImport = pragmaArgs[1]
        asmBlock = getAsmBlock(asmFileText, funcToImport + ":")
        codeBytes = blockToBytes(asmBlock)

        isGlobal = True
        if args.scope and f".global {funcToImport}" not in asmFileText:
            isGlobal = False

        noSig = False
        if len(pragmaArgs) > 2 and pragmaArgs[2] == "nosig":
            noSig = True

        newSource = ""
        newSource = writeCode(newSource, funcToImport, codeBytes, isGlobal, noSig)
        sourceText = sourceText.replace(replacePragmaText, newSource)

        fileCache[hashKey] = newSource

    open(outPath, "w").write(sourceText)

    if fileCache["size"] != len(fileCache):
        fileCache["size"] = len(fileCache)
        fileCache["name"] = str(sourcePath)
        saveFileCache(sourcePath, fileCache)


run()
