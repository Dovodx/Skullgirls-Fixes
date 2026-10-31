// Show the raw value stored at given addresses, read as several types. This is
// useful for checking constants such as timeouts.
//
// Usage: -postScript DumpValues.java <addresses.txt> <output.tsv>
//
// addresses.txt has one hex address per line. Text after the address is ignored,
// and lines starting with '#' are skipped.
//
// Output columns: address, 32-bit value, 64-bit value, double, float.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryAccessException;

import java.io.*;
import java.nio.file.*;

public class DumpValues extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("usage: DumpValues.java <addresses.txt> <output.tsv>");
            return;
        }
        Memory memory = currentProgram.getMemory();
        try (PrintWriter out = new PrintWriter(new FileWriter(args[1]))) {
            for (String line : Files.readAllLines(Paths.get(args[0]))) {
                line = line.strip();
                if (line.isEmpty() || line.startsWith("#")) {
                    continue;
                }
                Address address = toAddr(line.split("\\s+")[0]);
                try {
                    int value32 = memory.getInt(address);
                    long value64 = memory.getLong(address);
                    out.printf("%s\t0x%08x\t0x%016x\t%g\t%g%n", address, value32, value64,
                        Double.longBitsToDouble(value64), Float.intBitsToFloat(value32));
                } catch (MemoryAccessException e) {
                    out.printf("%s\tunreadable%n", address);
                }
            }
        }
    }
}
