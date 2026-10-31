// Write disassembly to .asm files. Instructions are annotated with the strings,
// functions, symbols and constant values they use.
//
// Usage: -postScript DumpDisassembly.java <requests.txt> <output_dir>
//
// Each line of requests.txt is one request (lines starting with '#' are skipped):
//   <hex_address>            the whole function containing this address
//   <hex_address> +<count>   count instructions starting at this address
//   <hex_start> <hex_end>    every instruction in this address range
//
// A whole-function file starts with a list of what references that function.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class DumpDisassembly extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("usage: DumpDisassembly.java <requests.txt> <output_dir>");
            return;
        }
        Path outputDir = Paths.get(args[1]);
        Files.createDirectories(outputDir);

        for (String line : Files.readAllLines(Paths.get(args[0]))) {
            line = line.strip();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            String[] fields = line.split("\\s+");
            Address start = toAddr(fields[0]);

            if (fields.length == 1) {
                dumpFunction(start, outputDir);
            } else if (fields[1].startsWith("+")) {
                int count = Integer.parseInt(fields[1].substring(1));
                dumpInstructionCount(start, count, outputDir);
            } else {
                AddressSetView range = currentProgram.getAddressFactory()
                    .getAddressSet(start, toAddr(fields[1]));
                try (PrintWriter out = openOutput(outputDir, start + "_range.asm")) {
                    writeInstructions(out, currentProgram.getListing().getInstructions(range, true));
                }
            }
        }
    }

    private PrintWriter openOutput(Path outputDir, String fileName) throws IOException {
        return new PrintWriter(Files.newBufferedWriter(outputDir.resolve(fileName)));
    }

    private void dumpFunction(Address address, Path outputDir) throws IOException {
        Function function = getFunctionContaining(address);
        if (function == null) {
            printerr("no function at " + address);
            return;
        }
        String fileName = function.getEntryPoint() + "_" + function.getName() + ".asm";
        try (PrintWriter out = openOutput(outputDir, fileName)) {
            out.println("; " + function.getName() + " @ " + function.getEntryPoint());
            for (Reference reference : getReferencesTo(function.getEntryPoint())) {
                Function caller = getFunctionContaining(reference.getFromAddress());
                out.println(";   referenced from " + reference.getFromAddress() + " "
                    + (caller == null ? "-" : caller.getName()) + " " + reference.getReferenceType());
            }
            writeInstructions(out, currentProgram.getListing().getInstructions(function.getBody(), true));
        }
    }

    private void dumpInstructionCount(Address start, int count, Path outputDir) throws IOException {
        Listing listing = currentProgram.getListing();
        Instruction instruction = listing.getInstructionAt(start);
        if (instruction == null) {
            instruction = listing.getInstructionAfter(start);
        }
        try (PrintWriter out = openOutput(outputDir, start + "_count.asm")) {
            for (int i = 0; i < count && instruction != null; i++) {
                out.println(formatInstruction(instruction));
                instruction = instruction.getNext();
            }
        }
    }

    private void writeInstructions(PrintWriter out, InstructionIterator instructions) {
        while (instructions.hasNext()) {
            out.println(formatInstruction(instructions.next()));
        }
    }

    private String formatInstruction(Instruction instruction) {
        // A set, because an operand and a reference often point at the same thing.
        Set<String> notes = new LinkedHashSet<>();

        for (Reference reference : instruction.getReferencesFrom()) {
            // Skip jumps. They only point to other lines in this function.
            if (reference.getReferenceType().isFlow() && !reference.getReferenceType().isCall()) {
                continue;
            }
            notes.add(describe(reference.getToAddress()));
        }

        // Constants can also be addresses, such as a string pushed as an argument.
        for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
            for (Object part : instruction.getOpObjects(operand)) {
                if (part instanceof Scalar scalar) {
                    long value = scalar.getUnsignedValue();
                    if (value <= 0xffffffffL && isInDataSection(toAddr(value))) {
                        notes.add(describe(toAddr(value)));
                    }
                }
            }
        }

        notes.remove("");
        StringBuilder line = new StringBuilder(instruction.getAddress() + "  " + instruction);
        for (String note : notes) {
            line.append(" ; ").append(note);
        }
        return line.toString();
    }

    private boolean isInDataSection(Address address) {
        MemoryBlock block = currentProgram.getMemory().getBlock(address);
        return block != null && (block.getName().equals(".rdata") || block.getName().equals(".data"));
    }

    /** Returns a note for an address: a string, name, or raw constant. Empty if unknown. */
    private String describe(Address address) {
        Data data = getDataAt(address);
        if (data == null) {
            data = getDataContaining(address);
        }
        if (data != null && data.hasStringValue()) {
            return "\"" + String.valueOf(data.getValue()).replace("\n", "\\n") + "\"";
        }
        Function function = getFunctionAt(address);
        if (function != null) {
            return "fn " + function.getName();
        }

        MemoryBlock block = currentProgram.getMemory().getBlock(address);
        String constant = "";
        // .rdata values are often tuning constants, so show what they contain.
        if (block != null && block.getName().equals(".rdata")) {
            constant = describeConstant(address);
        }
        Symbol symbol = getSymbolAt(address);
        return ((symbol == null ? "" : symbol.getName() + " ") + constant).strip();
    }

    private String describeConstant(Address address) {
        try {
            int value32 = currentProgram.getMemory().getInt(address);
            long value64 = currentProgram.getMemory().getLong(address);
            return String.format("[dword=0x%08x double=%g float=%g]", value32,
                Double.longBitsToDouble(value64), Float.intBitsToFloat(value32));
        } catch (MemoryAccessException e) {
            return "";
        }
    }
}
