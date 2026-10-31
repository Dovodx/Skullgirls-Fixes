// Give auto-named functions (FUN_xxxxxxxx) readable names based on their log
// messages. Many messages start with the name of the function that prints them, for
// example "UpdateGGPO: ggpo_add_local_input Failed".
//
// A function is renamed only if all of its tagged messages agree on one name. The
// new name keeps the address as a suffix, for example UpdateGGPO_0053bda0.
//
// Usage (headless): -postScript LabelFromLogStrings.java <output.tsv>
//
// Output columns: entry address, old name, new name, all tags found.
// Note: this script changes the project, so run it without -readOnly to keep the names.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.util.exception.*;

import java.io.*;
import java.util.*;
import java.util.regex.*;

public class LabelFromLogStrings extends GhidraScript {
    // Matches a leading "Name: ", "Name::Method: " or "Name(): ".
    private static final Pattern TAG =
        Pattern.compile("^([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)?)(?:\\(\\))?:\\s");

    // Words that look like function-name tags in log messages but aren't.
    private static final Set<String> NOT_FUNCTION_NAMES = Set.of("ERROR", "Error", "Warning", "WARNING",
        "Note", "FRAMESKIP", "Frameskip", "Spectator", "Debug", "INFO", "http", "https");

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            printerr("usage: LabelFromLogStrings.java <output.tsv>");
            return;
        }
        Map<Function, Set<String>> tagsByFunction = collectTags();

        int renamedCount = 0;
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("entry\told_name\tnew_name\ttags");
            for (var entry : tagsByFunction.entrySet()) {
                Function function = entry.getKey();
                Set<String> tags = entry.getValue();
                String oldName = function.getName();

                // Only rename when every tag agrees, and never replace a real name.
                String result = "-";
                if (tags.size() == 1) {
                    result = tags.iterator().next().replace("::", "__");
                    if (oldName.startsWith("FUN_")) {
                        try {
                            function.setName(result + "_" + function.getEntryPoint(), SourceType.ANALYSIS);
                            renamedCount++;
                        } catch (DuplicateNameException | InvalidInputException e) {
                            printerr("could not rename " + oldName + ": " + e.getMessage());
                        }
                    }
                    result = function.getName();
                }
                out.printf("%s\t%s\t%s\t%s%n", function.getEntryPoint(), oldName, result, tags);
            }
        }
        println("LabelFromLogStrings: renamed " + renamedCount + " functions -> " + args[0]);
    }

    /** Finds every log-message tag and the functions that use it. */
    private Map<Function, Set<String>> collectTags() {
        Map<Function, Set<String>> tagsByFunction = new HashMap<>();
        DataIterator allData = currentProgram.getListing().getDefinedData(true);
        while (allData.hasNext()) {
            Data data = allData.next();
            if (!data.hasStringValue() || data.getValue() == null) {
                continue;
            }
            Matcher matcher = TAG.matcher(data.getValue().toString());
            if (!matcher.find() || NOT_FUNCTION_NAMES.contains(matcher.group(1))) {
                continue;
            }
            for (Reference reference : getReferencesTo(data.getAddress())) {
                Function function = getFunctionContaining(reference.getFromAddress());
                if (function != null) {
                    tagsByFunction.computeIfAbsent(function, key -> new TreeSet<>()).add(matcher.group(1));
                }
            }
        }
        return tagsByFunction;
    }
}
