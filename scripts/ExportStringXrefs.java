// Find text strings in the game that match any of a list of patterns, and list
// every function that uses each one. The game logs a lot, so its log messages are a
// quick way to find the code for a feature.
//
// Usage (headless): -postScript ExportStringXrefs.java <keywords.txt> <output.tsv>
//
// keywords.txt has one case-insensitive Java regex per line. Lines starting with '#'
// are skipped.
//
// Output columns: string address, function entry, function name, reference address,
// string. Strings that nothing references get one row with '-' in those columns.
//@category SkullgirlsRE

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

public class ExportStringXrefs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("usage: ExportStringXrefs.java <keywords.txt> <output.tsv>");
            return;
        }
        List<Pattern> patterns = readPatterns(args[0]);
        int stringCount = 0;
        int referenceCount = 0;

        try (PrintWriter out = new PrintWriter(new FileWriter(args[1]))) {
            out.println("string_addr\tfunc_entry\tfunc_name\tref_addr\tstring");
            DataIterator allData = currentProgram.getListing().getDefinedData(true);
            while (allData.hasNext() && !monitor.isCancelled()) {
                Data data = allData.next();
                if (!data.hasStringValue() || data.getValue() == null) {
                    continue;
                }
                String text = data.getValue().toString();
                if (!matchesAny(patterns, text)) {
                    continue;
                }
                stringCount++;
                referenceCount += writeReferences(out, data.getAddress(), text);
            }
        }
        println("ExportStringXrefs: " + stringCount + " strings, " + referenceCount + " refs -> " + args[1]);
    }

    private List<Pattern> readPatterns(String path) throws IOException {
        List<Pattern> patterns = new ArrayList<>();
        for (String line : Files.readAllLines(Paths.get(path))) {
            line = line.strip();
            if (!line.isEmpty() && !line.startsWith("#")) {
                patterns.add(Pattern.compile(line, Pattern.CASE_INSENSITIVE));
            }
        }
        return patterns;
    }

    private boolean matchesAny(List<Pattern> patterns, String text) {
        return patterns.stream().anyMatch(pattern -> pattern.matcher(text).find());
    }

    /** Writes one row per reference and returns how many references were found. */
    private int writeReferences(PrintWriter out, Address stringAddress, String text) {
        // Keep each string on one row of the TSV.
        String escaped = text.replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r");

        int count = 0;
        for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(stringAddress)) {
            Address from = reference.getFromAddress();
            Function function = getFunctionContaining(from);
            out.printf("%s\t%s\t%s\t%s\t%s%n", stringAddress,
                function == null ? "-" : function.getEntryPoint(),
                function == null ? "-" : function.getName(), from, escaped);
            count++;
        }
        if (count == 0) {
            out.printf("%s\t-\t-\t-\t%s%n", stringAddress, escaped);
        }
        return count;
    }
}
