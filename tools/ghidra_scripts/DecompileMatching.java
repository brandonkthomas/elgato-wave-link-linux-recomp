// Decompile every function whose name matches a regex (or exact addresses) into one file each.
// Args: <outDir> <regex-or-0xaddr> [more...]
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

public class DecompileMatching extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File outDir = new File(args[0]);
        outDir.mkdirs();
        List<Function> targets = new ArrayList<>();
        for (int i = 1; i < args.length; i++) {
            String a = args[i];
            if (a.startsWith("0x")) {
                Address addr = toAddr(a);
                Function f = getFunctionContaining(addr);
                if (f != null) targets.add(f);
                else println("no function at " + a);
            } else {
                Pattern p = Pattern.compile(a);
                for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                    if (p.matcher(f.getName(true)).find()) targets.add(f);
                }
            }
        }
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        for (Function f : targets) {
            DecompileResults r = ifc.decompileFunction(f, 300, monitor);
            String name = f.getName(true).replaceAll("[^A-Za-z0-9_.-]", "_");
            if (name.length() > 120) name = name.substring(0, 120);
            File out = new File(outDir, f.getEntryPoint() + "_" + name + ".c");
            try (PrintWriter w = new PrintWriter(out)) {
                w.println("// " + f.getName(true) + " @ " + f.getEntryPoint());
                if (r.decompileCompleted()) w.println(r.getDecompiledFunction().getC());
                else w.println("// decompile failed: " + r.getErrorMessage());
            }
            println("wrote " + out);
        }
        ifc.dispose();
    }
}
