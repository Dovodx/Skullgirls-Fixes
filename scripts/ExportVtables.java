// List each C++ class's virtual function table (vftable), so that a class name can
// be matched to the functions that implement it.
//
// This needs Ghidra's RTTI analyzer to have run, because it creates the "vftable"
// labels this script looks for. RTTI is the type information MSVC stores in the exe.
//
// Usage (headless): -postScript ExportVtables.java <output.tsv>
//
// Output columns: class, vftable address, slot index, target address, target name.
// Note: this script can change the project, because it creates a function at any
// slot target that doesn't have one yet.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressOutOfBoundsException;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.symbol.*;

import java.io.*;

public class ExportVtables extends GhidraScript {
    private static final int POINTER_SIZE = 4;
    private static final int MAX_SLOTS = 512;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            printerr("usage: ExportVtables.java <output.tsv>");
            return;
        }
        int slotCount = 0;
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("class\tvftable\tslot\ttarget\ttarget_name");
            for (Symbol symbol : currentProgram.getSymbolTable().getAllSymbols(true)) {
                if (symbol.getName().startsWith("vftable")) {
                    slotCount += writeTable(out, symbol);
                }
            }
        }
        println("ExportVtables: " + slotCount + " slots -> " + args[0]);
    }

    /** Writes one row per slot in a table and returns the number of slots. */
    private int writeTable(PrintWriter out, Symbol table) throws MemoryAccessException {
        String className = table.getParentNamespace().getName(true);
        Address tableAddress = table.getAddress();
        Memory memory = currentProgram.getMemory();

        int slot = 0;
        while (slot < MAX_SLOTS) {
            Address slotAddress = tableAddress.add(slot * (long) POINTER_SIZE);
            // A label here means the next table or its RTTI data starts, so this table ended.
            boolean nextTableStarts = slot > 0
                && currentProgram.getSymbolTable().getPrimarySymbol(slotAddress) != null;
            if (nextTableStarts || !memory.contains(slotAddress)) {
                break;
            }
            Address target = readCodePointer(slotAddress);
            if (target == null) {
                break;
            }
            Function function = getOrCreateFunction(target);
            out.printf("%s\t%s\t%d\t%s\t%s%n", className, tableAddress, slot, target,
                function == null ? "-" : function.getName(true));
            slot++;
        }
        return slot;
    }

    /** Returns the code address stored in a slot, or null if it doesn't point at code. */
    private Address readCodePointer(Address slotAddress) throws MemoryAccessException {
        Memory memory = currentProgram.getMemory();
        long value = memory.getInt(slotAddress) & 0xffffffffL;
        Address target;
        try {
            target = toAddr(value);
        } catch (AddressOutOfBoundsException e) {
            return null;
        }
        boolean pointsAtCode = memory.contains(target) && memory.getBlock(target).isExecute();
        return pointsAtCode ? target : null;
    }

    /** Creates a missing function so that later scripts can decompile it. */
    private Function getOrCreateFunction(Address address) {
        Function function = getFunctionAt(address);
        if (function != null) {
            return function;
        }
        try {
            return createFunction(address, null);
        } catch (RuntimeException e) {
            return null;
        }
    }
}
