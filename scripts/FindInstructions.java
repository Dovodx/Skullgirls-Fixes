// Find every instruction that matches a search. This is useful for finding code
// that touches a known struct field or constant.
//
// Usage: -postScript FindInstructions.java <output.txt> <mode> <value> [value ...]
//
// Modes:
//   operand  Each value is a hex number, matched against instruction operands.
//            To find uses of struct field +0x44c, search for 44c. Any operand with
//            that value counts, so expect some unrelated matches.
//   text     Each value is exact text to find in the instruction as Ghidra prints
//            it, for example "+ 0x3f8]".
//   regex    Each value is a case-insensitive regular expression, matched against
//            the instruction as Ghidra prints it.
//
// Output columns: search value, instruction address, function, instruction.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;

import java.io.*;
import java.util.*;
import java.util.function.Predicate;
import java.util.regex.Pattern;

public class FindInstructions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3) {
            printerr("usage: FindInstructions.java <output.txt> <operand|text|regex> <value> [value ...]");
            return;
        }
        String mode = args[1];

        // Keep insertion order so that results are grouped consistently.
        Map<String, Predicate<Instruction>> searches = new LinkedHashMap<>();
        for (String value : Arrays.asList(args).subList(2, args.length)) {
            searches.put(value, buildSearch(mode, value));
        }

        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            InstructionIterator instructions = currentProgram.getListing().getInstructions(true);
            while (instructions.hasNext() && !monitor.isCancelled()) {
                Instruction instruction = instructions.next();
                for (var search : searches.entrySet()) {
                    if (search.getValue().test(instruction)) {
                        Function function = getFunctionContaining(instruction.getAddress());
                        String functionText = function == null
                            ? "-" : function.getName() + "@" + function.getEntryPoint();
                        out.println(search.getKey() + "\t" + instruction.getAddress() + "\t"
                            + functionText + "\t" + instruction);
                    }
                }
            }
        }
    }

    private Predicate<Instruction> buildSearch(String mode, String value) {
        switch (mode) {
            case "operand":
                long number = Long.parseLong(value, 16);
                return instruction -> hasOperandValue(instruction, number);
            case "text":
                return instruction -> instruction.toString().contains(value);
            case "regex":
                Pattern pattern = Pattern.compile(value, Pattern.CASE_INSENSITIVE);
                return instruction -> pattern.matcher(instruction.toString()).find();
            default:
                throw new IllegalArgumentException("unknown mode: " + mode);
        }
    }

    private boolean hasOperandValue(Instruction instruction, long number) {
        for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
            for (Object part : instruction.getOpObjects(operand)) {
                if (part instanceof Scalar scalar && scalar.getUnsignedValue() == number) {
                    return true;
                }
            }
        }
        return false;
    }
}
