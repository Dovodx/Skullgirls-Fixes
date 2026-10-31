// Decompile a list of functions to C-like text, one .c file per function.
// Optionally follows calls, so the functions they call are decompiled too.
//
// Usage (headless): -postScript DumpDecomp.java <addresses.txt> <output_dir> [call_depth]
//
// addresses.txt has one hex address per line. Any address inside a function works.
// Text after the address is ignored, and lines starting with '#' are skipped.
// call_depth is how many levels of called functions to follow (default 0: none).
//
// Also appends to index.tsv in the output folder: entry address, name, depth.
//@category SkullgirlsRE

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class DumpDecomp extends GhidraScript {
    private static final int DECOMPILE_TIMEOUT_SECONDS = 120;

    /** A function waiting to be decompiled, and how many calls it is from the input list. */
    private record WorkItem(Function function, int depth) {
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("usage: DumpDecomp.java <addresses.txt> <output_dir> [call_depth]");
            return;
        }
        int maxDepth = args.length > 2 ? Integer.parseInt(args[2]) : 0;
        Path outputDir = Paths.get(args[1]);
        Files.createDirectories(outputDir);

        Deque<WorkItem> queue = readInputFunctions(args[0]);
        Set<Address> finished = new HashSet<>();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        try (PrintWriter index = new PrintWriter(new FileWriter(outputDir.resolve("index.tsv").toFile(), true))) {
            while (!queue.isEmpty() && !monitor.isCancelled()) {
                WorkItem item = queue.poll();
                Function function = item.function();
                if (function.isThunk() || function.isExternal() || !finished.add(function.getEntryPoint())) {
                    continue;
                }
                writeDecompiledFunction(decompiler, function, outputDir);
                index.printf("%s\t%s\t%d%n", function.getEntryPoint(), function.getName(true), item.depth());

                if (item.depth() < maxDepth) {
                    for (Function called : function.getCalledFunctions(monitor)) {
                        queue.add(new WorkItem(called, item.depth() + 1));
                    }
                }
            }
        }
        decompiler.dispose();
        println("DumpDecomp: " + finished.size() + " functions -> " + outputDir);
    }

    private Deque<WorkItem> readInputFunctions(String path) throws IOException {
        Deque<WorkItem> items = new ArrayDeque<>();
        for (String line : Files.readAllLines(Paths.get(path))) {
            line = line.strip();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            Address address = toAddr(line.split("\\s+")[0]);
            Function function = getFunctionContaining(address);
            if (function == null) {
                printerr("no function at " + address);
                continue;
            }
            items.add(new WorkItem(function, 0));
        }
        return items;
    }

    private void writeDecompiledFunction(DecompInterface decompiler, Function function, Path outputDir)
            throws IOException {
        DecompileResults results = decompiler.decompileFunction(function, DECOMPILE_TIMEOUT_SECONDS, monitor);
        String code = results.decompileCompleted()
            ? results.getDecompiledFunction().getC()
            : "// decompile failed: " + results.getErrorMessage() + "\n";
        String safeName = function.getName().replaceAll("[^A-Za-z0-9_.]", "_");
        String fileName = function.getEntryPoint() + "_" + safeName + ".c";
        String header = "// " + function.getName(true) + " @ " + function.getEntryPoint() + "\n";
        Files.writeString(outputDir.resolve(fileName), header + code);
    }
}
