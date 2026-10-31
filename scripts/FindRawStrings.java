// Find exact text strings by searching raw memory, then list every reference to any
// byte of each match. Unlike ExportStringXrefs, this also finds strings Ghidra didn't
// define, and references that point into the middle of a string. The compiler
// sometimes copies strings in pieces, which produces references like that.
//
// Usage: -postScript FindRawStrings.java <output.txt> <string> [string ...]
//
// Output: a "## string @ address" heading per match, then one row per reference:
// reference address, function, byte offset into the string, instruction.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.symbol.Reference;

import java.io.*;
import java.nio.charset.StandardCharsets;

public class FindRawStrings extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("usage: FindRawStrings.java <output.txt> <string> [string ...]");
            return;
        }
        Memory memory = currentProgram.getMemory();
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (int i = 1; i < args.length; i++) {
                // Include the terminating NUL so that the whole string has to match.
                byte[] searchBytes = (args[i] + "\0").getBytes(StandardCharsets.US_ASCII);

                Address searchFrom = currentProgram.getMinAddress();
                Address match;
                while ((match = memory.findBytes(searchFrom, searchBytes, null, true, monitor)) != null) {
                    if (isStartOfString(match)) {
                        out.println("## \"" + args[i] + "\" @ " + match);
                        writeReferences(out, match, searchBytes.length);
                    }
                    searchFrom = match.add(1);
                }
            }
        }
    }

    /** Checks for a NUL before the match, so "Main" won't match the end of "ToMain". */
    private boolean isStartOfString(Address address) {
        try {
            return currentProgram.getMemory().getByte(address.subtract(1)) == 0;
        } catch (MemoryAccessException e) {
            return false;
        }
    }

    private void writeReferences(PrintWriter out, Address stringStart, int length) {
        for (int offset = 0; offset < length; offset++) {
            for (Reference reference : getReferencesTo(stringStart.add(offset))) {
                Address from = reference.getFromAddress();
                Function function = getFunctionContaining(from);
                out.println(from + "\t" + (function == null ? "-" : function.getName())
                    + "\t+" + offset + "\t" + getInstructionAt(from));
            }
        }
    }
}
