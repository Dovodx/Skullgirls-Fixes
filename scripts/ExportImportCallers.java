// List the game functions that call given imported functions, such as Windows or
// Steam API calls like QueryPerformanceCounter.
//
// Usage (headless): -postScript ExportImportCallers.java <output.tsv> <name1> [name2 ...]
//
// Output columns: import name, caller entry address, caller name, reference address.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

import java.io.*;
import java.util.*;

public class ExportImportCallers extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("usage: ExportImportCallers.java <output.tsv> <name1> [name2 ...]");
            return;
        }
        Set<String> wantedNames = new HashSet<>(Arrays.asList(args).subList(1, args.length));

        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("import\tfunc_entry\tfunc_name\tref_addr");
            for (Symbol imported : currentProgram.getSymbolTable().getExternalSymbols()) {
                if (wantedNames.contains(imported.getName())) {
                    writeCallers(out, imported);
                }
            }
        }
        println("ExportImportCallers -> " + args[0]);
    }

    /**
     * Imported functions are rarely referenced directly. Callers usually go through
     * a pointer in the Import Address Table (IAT), or a small thunk function that
     * jumps to it. This starts from those references and follows them back to real
     * callers.
     */
    private void writeCallers(PrintWriter out, Symbol imported) {
        Deque<Address> toVisit = new ArrayDeque<>();
        addReferencesTo(imported.getAddress(), toVisit);

        Set<Address> visited = new HashSet<>();
        while (!toVisit.isEmpty()) {
            Address address = toVisit.poll();
            if (!visited.add(address)) {
                continue;
            }
            Function function = getFunctionContaining(address);
            if (function == null) {
                // Not inside a function, so this is an IAT pointer.
                addReferencesTo(address, toVisit);
            } else if (function.isThunk()) {
                // A thunk has no logic of its own, so follow what calls it.
                addReferencesTo(function.getEntryPoint(), toVisit);
            } else {
                out.printf("%s\t%s\t%s\t%s%n", imported.getName(),
                    function.getEntryPoint(), function.getName(), address);
            }
        }
    }

    private void addReferencesTo(Address address, Deque<Address> toVisit) {
        for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(address)) {
            toVisit.add(reference.getFromAddress());
        }
    }
}
