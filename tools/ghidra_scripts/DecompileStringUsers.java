// Decompile every function that references a defined string containing one of the given substrings.
// Args: <outDir> <substring> [more...]
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.io.File;
import java.io.PrintWriter;
import java.util.LinkedHashMap;
import java.util.Map;

public class DecompileStringUsers extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File outDir = new File(args[0]);
        outDir.mkdirs();
        Map<Function, String> targets = new LinkedHashMap<>();
        for (Data d : currentProgram.getListing().getDefinedData(true)) {
            if (!d.hasStringValue()) continue;
            String s = String.valueOf(d.getValue());
            for (int i = 1; i < args.length; i++) {
                if (!s.contains(args[i])) continue;
                for (Reference r : getReferencesTo(d.getAddress())) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    if (f != null) targets.putIfAbsent(f, s);
                }
            }
        }
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        for (Map.Entry<Function, String> e : targets.entrySet()) {
            Function f = e.getKey();
            DecompileResults r = ifc.decompileFunction(f, 300, monitor);
            File out = new File(outDir, f.getEntryPoint() + "_" + f.getName().replaceAll("[^A-Za-z0-9_.-]", "_") + ".c");
            try (PrintWriter w = new PrintWriter(out)) {
                w.println("// " + f.getName(true) + " @ " + f.getEntryPoint() + " (references \"" + e.getValue() + "\")");
                if (r.decompileCompleted()) w.println(r.getDecompiledFunction().getC());
                else w.println("// decompile failed: " + r.getErrorMessage());
            }
            println("wrote " + out);
        }
        ifc.dispose();
    }
}
