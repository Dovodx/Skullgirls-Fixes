// Make Ghidra work out function parameters and save them to the project, which makes
// call arguments much easier to read in decompiled code. It covers each function in
// the given address ranges and the functions they call.
//
// Usage: -postScript CommitFunctionParameters.java <start> <end> [<start> <end> ...]
//
// Note: this script changes the project, so run it without -readOnly to keep the
// results. It skips signatures that were set by hand or imported.
//@category SkullgirlsRE

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.SourceType;
import ghidra.util.exception.*;

import java.util.*;

public class CommitFunctionParameters extends GhidraScript {
    private static final int DECOMPILE_TIMEOUT_SECONDS = 60;
    // Two passes, because parameters found in one function can improve its callers.
    private static final int PASSES = 2;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2 || args.length % 2 != 0) {
            printerr("usage: CommitFunctionParameters.java <start> <end> [<start> <end> ...]");
            return;
        }
        AddressSet ranges = new AddressSet();
        for (int i = 0; i < args.length; i += 2) {
            ranges.add(toAddr(args[i]), toAddr(args[i + 1]));
        }

        Set<Function> functions = collectFunctions(ranges);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        int committed = 0;
        for (int pass = 0; pass < PASSES; pass++) {
            for (Function function : functions) {
                if (monitor.isCancelled()) {
                    break;
                }
                if (commitParameters(decompiler, function)) {
                    committed++;
                }
            }
        }
        decompiler.dispose();
        println("CommitFunctionParameters: committed " + committed + " over "
            + functions.size() + " functions");
    }

    /** Lists called functions first, so the functions that call them see their results. */
    private Set<Function> collectFunctions(AddressSet ranges) throws CancelledException {
        FunctionManager functionManager = currentProgram.getFunctionManager();
        Set<Function> functions = new LinkedHashSet<>();
        for (Function function : functionManager.getFunctions(ranges, true)) {
            for (Function called : function.getCalledFunctions(monitor)) {
                if (!called.isExternal() && !called.isThunk()) {
                    functions.add(called);
                }
            }
        }
        for (Function function : functionManager.getFunctions(ranges, true)) {
            functions.add(function);
        }
        return functions;
    }

    private boolean commitParameters(DecompInterface decompiler, Function function) {
        SourceType source = function.getSignatureSource();
        if (source == SourceType.USER_DEFINED || source == SourceType.IMPORTED) {
            return false;
        }
        DecompileResults results =
            decompiler.decompileFunction(function, DECOMPILE_TIMEOUT_SECONDS, monitor);
        if (!results.decompileCompleted()) {
            return false;
        }
        try {
            HighFunctionDBUtil.commitParamsToDatabase(results.getHighFunction(), false,
                HighFunctionDBUtil.ReturnCommitOption.COMMIT, SourceType.ANALYSIS);
            return true;
        } catch (DuplicateNameException | InvalidInputException e) {
            printerr("could not commit " + function.getName() + ": " + e.getMessage());
            return false;
        }
    }
}
