// Find methods called on a global object.
//
// On 32-bit MSVC builds, the "this" pointer for a method call goes in the ECX register.
// So a call shortly after "MOV ECX,<object address>" is a call to one of that object's
// methods. This script finds those calls and groups them by the function called.
//
// Usage: -postScript FindMethodCallsOnObject.java <output.txt> <hex_object_address> [max_lookback]
//
// max_lookback is how many instructions to look back from each call (default 6).
//
// Output columns: called function, number of calls, call sites. Each site is printed
// as address(containing function), and long lists are cut off.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;

import java.io.*;
import java.util.*;

public class FindMethodCallsOnObject extends GhidraScript {
    private static final int DEFAULT_MAX_LOOKBACK = 6;
    private static final int MAX_SITE_LIST_LENGTH = 300;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("usage: FindMethodCallsOnObject.java <output.txt> <hex_object_address> [max_lookback]");
            return;
        }
        String loadObject = "MOV ECX,0x" + Long.toHexString(Long.parseLong(args[1], 16));
        int maxLookback = args.length > 2 ? Integer.parseInt(args[2]) : DEFAULT_MAX_LOOKBACK;

        Map<String, Integer> callCounts = new TreeMap<>();
        Map<String, String> callSites = new HashMap<>();

        InstructionIterator instructions = currentProgram.getListing().getInstructions(true);
        while (instructions.hasNext()) {
            Instruction instruction = instructions.next();
            if (!instruction.getFlowType().isCall()
                    || !setsThisPointer(instruction, loadObject, maxLookback)) {
                continue;
            }
            Address target = callTarget(instruction);
            if (target == null) {
                continue;
            }
            Function called = getFunctionAt(target);
            String key = target + " " + (called == null ? "?" : called.getName());
            Function caller = getFunctionContaining(instruction.getAddress());
            String site = instruction.getAddress() + "(" + (caller == null ? "-" : caller.getEntryPoint()) + ")";

            callCounts.merge(key, 1, Integer::sum);
            callSites.merge(key, site, (list, added) ->
                list.length() > MAX_SITE_LIST_LENGTH ? list : list + " " + added);
        }

        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (String key : callCounts.keySet()) {
                out.println(key + "\t" + callCounts.get(key) + "\t" + callSites.get(key));
            }
        }
    }

    /** Looks back from a call for the instruction that last set ECX. */
    private boolean setsThisPointer(Instruction call, String loadObject, int maxLookback) {
        Instruction earlier = call;
        for (int i = 0; i < maxLookback; i++) {
            earlier = earlier.getPrevious();
            if (earlier == null) {
                return false;
            }
            String text = earlier.toString();
            if (text.startsWith(loadObject)) {
                return true;
            }
            // ECX was set to something else, so this call isn't on our object.
            if (text.startsWith("MOV ECX,") || text.startsWith("LEA ECX,")) {
                return false;
            }
        }
        return false;
    }

    private Address callTarget(Instruction call) {
        Address target = null;
        for (Reference reference : call.getReferencesFrom()) {
            if (reference.getReferenceType().isCall()) {
                target = reference.getToAddress();
            }
        }
        return target;
    }
}
