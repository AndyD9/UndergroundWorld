// Static, read-only inspection of the existing Ghidra database.
// @category UndergroundWorld
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import com.google.gson.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class ExportCandidates extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("output.json address...");
        String expected = "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7";
        if (!expected.equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new IllegalStateException("Wrong database identity");
        JsonObject root = new JsonObject();
        root.addProperty("sha256", currentProgram.getExecutableSHA256());
        root.addProperty("language", currentProgram.getLanguageID().toString());
        root.addProperty("image_base", currentProgram.getImageBase().toString());
        root.addProperty("method", "Ghidra 12.1.4 existing auto-analysis; static xrefs and decompiler; no target execution");
        JsonArray rows = new JsonArray();
        DecompInterface decompiler = new DecompInterface();
        try {
            if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Decompiler unavailable");
            for (int n = 1; n < args.length; n++) {
                monitor.checkCancelled();
                Address address = toAddr(args[n]);
                if (address == null) throw new IllegalArgumentException("Invalid address: " + args[n]);
                JsonObject row = new JsonObject();
                row.addProperty("address", address.toString());
                JsonArray refs = new JsonArray();
                ReferenceIterator iter = currentProgram.getReferenceManager().getReferencesTo(address);
                int count = 0;
                while (iter.hasNext()) {
                    Reference ref = iter.next(); count++;
                    if (count > 1000) continue;
                    JsonObject r = new JsonObject();
                    r.addProperty("from", ref.getFromAddress().toString());
                    r.addProperty("type", ref.getReferenceType().toString());
                    Function owner = getFunctionContaining(ref.getFromAddress());
                    if (owner != null) r.addProperty("function", owner.getEntryPoint().toString());
                    Instruction instruction = getInstructionAt(ref.getFromAddress());
                    if (instruction != null) r.addProperty("instruction", instruction.toString());
                    refs.add(r);
                }
                row.add("references", refs);
                row.addProperty("reference_count", count);
                row.addProperty("references_truncated", count > 1000);
                Function f = getFunctionContaining(address);
                if (f != null) {
                    row.addProperty("function_entry", f.getEntryPoint().toString());
                    row.addProperty("function_name", f.getName());
                    DecompileResults result = decompiler.decompileFunction(f, 45, monitor);
                    row.addProperty("decompile_completed", result.decompileCompleted());
                    row.addProperty("decompile_error", result.getErrorMessage());
                    if (result.decompileCompleted() && result.getDecompiledFunction() != null)
                        row.addProperty("pseudocode", result.getDecompiledFunction().getC());
                    JsonArray assembly = new JsonArray();
                    InstructionIterator ins = currentProgram.getListing().getInstructions(f.getBody(), true);
                    int total = 0;
                    while (ins.hasNext()) {
                        Instruction i = ins.next(); total++;
                        if (total <= 3000) assembly.add(i.getAddress() + " " + i.toString());
                    }
                    row.add("assembly", assembly);
                    row.addProperty("instruction_count", total);
                    row.addProperty("assembly_truncated", total > 3000);
                }
                rows.add(row);
            }
        } finally { decompiler.dispose(); }
        root.add("targets", rows);
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        Files.writeString(output, new GsonBuilder().setPrettyPrinting().create().toJson(root), StandardCharsets.UTF_8);
        println("Exported " + rows.size() + " static targets to " + output);
    }
}
