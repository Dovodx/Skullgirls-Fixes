// List everything that references given addresses or strings.
//
// Usage: -postScript ListReferences.java <output.txt> <lines_before> <target> [target ...]
//
// lines_before is how many instructions to print before each reference. Use 0 for
// none. A few lines are usually enough to see the arguments pushed for a call.
//
// Each target is one of:
//   <hex_address>   a function or data address
//   s:<text>        every defined string containing this text
//   @<file>         a file of hex addresses, one per line (text after the address
//                   is ignored, and lines starting with '#' are skipped)
//
// Output: a "== address label" heading per target, then one row per reference:
// reference address, reference type (READ, WRITE, CALL...), function, instruction.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class ListReferences extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3) {
            printerr("usage: ListReferences.java <output.txt> <lines_before> <target> [target ...]");
            return;
        }
        int linesBefore = Integer.parseInt(args[1]);

        List<Address> targets = new ArrayList<>();
        for (String target : Arrays.asList(args).subList(2, args.length)) {
            targets.addAll(resolveTarget(target));
        }

        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (Address target : targets) {
                out.println("== " + target + " " + describe(target));
                for (Reference reference : getReferencesTo(target)) {
                    writeReference(out, reference, linesBefore);
                }
            }
        }
    }

    /** Turns one command-line target into the addresses it stands for. */
    private List<Address> resolveTarget(String target) throws IOException {
        List<Address> addresses = new ArrayList<>();
        if (target.startsWith("s:")) {
            String text = target.substring(2);
            for (Data data : currentProgram.getListing().getDefinedData(true)) {
                if (data.getValue() instanceof String value && value.contains(text)) {
                    addresses.add(data.getAddress());
                }
            }
        } else if (target.startsWith("@")) {
            for (String line : Files.readAllLines(Paths.get(target.substring(1)))) {
                line = line.strip();
                if (!line.isEmpty() && !line.startsWith("#")) {
                    addresses.add(toAddr(line.split("\\s+")[0]));
                }
            }
        } else {
            addresses.add(toAddr(target));
        }
        return addresses;
    }

    /** Returns a readable label for a target: its function name or string value. */
    private String describe(Address address) {
        Function function = getFunctionAt(address);
        if (function != null) {
            return function.getName();
        }
        Data data = getDataAt(address);
        return data != null ? String.valueOf(data.getValue()) : "";
    }

    private void writeReference(PrintWriter out, Reference reference, int linesBefore) {
        Address from = reference.getFromAddress();
        Instruction instruction = getInstructionAt(from);

        // Walk backwards, adding to the front to keep address order.
        Deque<String> earlierLines = new ArrayDeque<>();
        Instruction earlier = instruction;
        for (int i = 0; i < linesBefore && earlier != null; i++) {
            earlier = earlier.getPrevious();
            if (earlier != null) {
                earlierLines.addFirst("    " + earlier.getAddress() + "  " + earlier);
            }
        }
        earlierLines.forEach(out::println);

        Function function = getFunctionContaining(from);
        String functionText = function == null ? "-" : function.getName() + "@" + function.getEntryPoint();
        out.println(from + "\t" + reference.getReferenceType() + "\t" + functionText + "\t"
            + (instruction == null ? "" : instruction));
    }
}
