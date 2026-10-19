// Static, read-only queries on the existing Ghidra database (phase 3a).
// Queries: fn:VA (function with bytes, callers, pseudocode), refs:VA,
// bytes:VA:LEN, dwords:VA:COUNT, ptrscan:VA (4-byte value search),
// disp:HEX:START:END (memory operands with this displacement in a range),
// ins:START:END (listing instructions, even outside a defined function),
// pat:HEX (byte pattern, xx = any byte).
// @category UndergroundWorld
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.lang.OperandType;
import ghidra.program.model.symbol.*;
import com.google.gson.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class ExportQueries extends GhidraScript {
    private static final String EXPECTED = "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7";
    private DecompInterface decompiler;

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("output.json query...");
        if (!EXPECTED.equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new IllegalStateException("Wrong database identity");
        JsonObject root = new JsonObject();
        root.addProperty("sha256", currentProgram.getExecutableSHA256());
        root.addProperty("language", currentProgram.getLanguageID().toString());
        root.addProperty("image_base", currentProgram.getImageBase().toString());
        root.addProperty("method", "Ghidra 12.1.4 existing auto-analysis, read-only; static queries; no target execution");
        JsonArray rows = new JsonArray();
        decompiler = new DecompInterface();
        try {
            if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Decompiler unavailable");
            for (int n = 1; n < args.length; n++) {
                monitor.checkCancelled();
                String[] q = args[n].split(":");
                JsonObject row = new JsonObject();
                row.addProperty("query", args[n]);
                switch (q[0]) {
                    case "fn": function(row, addr(q[1])); break;
                    case "refs": refs(row, addr(q[1])); break;
                    case "bytes": bytes(row, addr(q[1]), Integer.parseInt(q[2])); break;
                    case "dwords": dwords(row, addr(q[1]), Integer.parseInt(q[2])); break;
                    case "ptrscan": ptrscan(row, Long.parseLong(q[1], 16)); break;
                    case "disp": disp(row, Long.parseLong(q[1], 16), addr(q[2]), addr(q[3])); break;
                    case "ins": instructions(row, addr(q[1]), addr(q[2])); break;
                    case "pat": pattern(row, q[1]); break;
                    default: throw new IllegalArgumentException("Unknown query: " + args[n]);
                }
                rows.add(row);
            }
        } finally { decompiler.dispose(); }
        root.add("results", rows);
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        Files.writeString(output, new GsonBuilder().setPrettyPrinting().create().toJson(root), StandardCharsets.UTF_8);
        println("Exported " + rows.size() + " static queries to " + output);
    }

    private Address addr(String text) {
        Address a = toAddr(text);
        if (a == null) throw new IllegalArgumentException("Invalid address: " + text);
        return a;
    }

    private String hex(byte[] data) {
        StringBuilder sb = new StringBuilder();
        for (byte b : data) sb.append(String.format("%02x", b & 0xff));
        return sb.toString();
    }

    private String line(Instruction i) throws Exception {
        return i.getAddress() + " " + hex(i.getBytes()) + " " + i;
    }

    private JsonArray refArray(Address a, int limit) {
        JsonArray refs = new JsonArray();
        ReferenceIterator iter = currentProgram.getReferenceManager().getReferencesTo(a);
        while (iter.hasNext() && refs.size() < limit) {
            Reference ref = iter.next();
            JsonObject r = new JsonObject();
            r.addProperty("from", ref.getFromAddress().toString());
            r.addProperty("type", ref.getReferenceType().toString());
            Function owner = getFunctionContaining(ref.getFromAddress());
            if (owner != null) r.addProperty("function", owner.getEntryPoint().toString());
            Instruction ins = getInstructionAt(ref.getFromAddress());
            if (ins != null) r.addProperty("instruction", ins.toString());
            refs.add(r);
        }
        return refs;
    }

    private void refs(JsonObject row, Address a) {
        JsonArray refs = refArray(a, 2000);
        row.add("references", refs);
        row.addProperty("reference_count", refs.size());
    }

    private void function(JsonObject row, Address a) throws Exception {
        Function f = getFunctionContaining(a);
        if (f == null) { row.addProperty("error", "no function"); return; }
        row.addProperty("function_entry", f.getEntryPoint().toString());
        row.add("callers", refArray(f.getEntryPoint(), 500));
        JsonArray assembly = new JsonArray();
        InstructionIterator ins = currentProgram.getListing().getInstructions(f.getBody(), true);
        int total = 0;
        while (ins.hasNext()) {
            Instruction i = ins.next(); total++;
            if (total <= 4000) assembly.add(line(i));
        }
        row.add("assembly", assembly);
        row.addProperty("instruction_count", total);
        row.addProperty("assembly_truncated", total > 4000);
        DecompileResults result = decompiler.decompileFunction(f, 60, monitor);
        row.addProperty("decompile_completed", result.decompileCompleted());
        if (result.decompileCompleted() && result.getDecompiledFunction() != null)
            row.addProperty("pseudocode", result.getDecompiledFunction().getC());
    }

    private void bytes(JsonObject row, Address a, int len) throws Exception {
        byte[] data = new byte[len];
        int got = currentProgram.getMemory().getBytes(a, data);
        row.addProperty("length", got);
        row.addProperty("hex", hex(data));
    }

    private void dwords(JsonObject row, Address a, int count) throws Exception {
        JsonArray values = new JsonArray();
        Memory mem = currentProgram.getMemory();
        for (int k = 0; k < count; k++) {
            Address at = a.add(4L * k);
            long v = mem.getInt(at) & 0xffffffffL;
            JsonObject o = new JsonObject();
            o.addProperty("at", at.toString());
            o.addProperty("value", String.format("%08x", v));
            Address target = toAddr(v);
            if (target != null && mem.contains(target)) {
                Function f = getFunctionAt(target);
                if (f != null) o.addProperty("function", f.getEntryPoint().toString());
            }
            values.add(o);
        }
        row.add("dwords", values);
    }

    private void ptrscan(JsonObject row, long value) throws Exception {
        byte[] pattern = { (byte) value, (byte) (value >> 8), (byte) (value >> 16), (byte) (value >> 24) };
        scan(row, pattern, null);
    }

    // Byte pattern in hexadecimal, "xx" for a wildcard byte.
    private void pattern(JsonObject row, String text) throws Exception {
        int n = text.length() / 2;
        byte[] bytes = new byte[n], mask = new byte[n];
        for (int k = 0; k < n; k++) {
            String b = text.substring(2 * k, 2 * k + 2);
            if (!b.equalsIgnoreCase("xx")) { bytes[k] = (byte) Integer.parseInt(b, 16); mask[k] = (byte) 0xff; }
        }
        scan(row, bytes, mask);
    }

    private void scan(JsonObject row, byte[] pattern, byte[] mask) throws Exception {
        JsonArray hits = new JsonArray();
        Memory mem = currentProgram.getMemory();
        for (MemoryBlock block : mem.getBlocks()) {
            if (!block.isInitialized()) continue;
            Address cur = block.getStart();
            while (cur != null && hits.size() < 500) {
                Address hit = mem.findBytes(cur, block.getEnd(), pattern, mask, true, monitor);
                if (hit == null) break;
                JsonObject o = new JsonObject();
                o.addProperty("at", hit.toString());
                o.addProperty("block", block.getName());
                Instruction ins = getInstructionContaining(hit);
                if (ins != null) o.addProperty("instruction", line(ins));
                Function f = getFunctionContaining(hit);
                if (f != null) o.addProperty("function", f.getEntryPoint().toString());
                hits.add(o);
                cur = hit.add(1);
            }
        }
        row.add("hits", hits);
    }

    // Instructions already present in the listing, even outside a defined function.
    private void instructions(JsonObject row, Address start, Address end) throws Exception {
        JsonArray assembly = new JsonArray();
        InstructionIterator it = currentProgram.getListing().getInstructions(new AddressSet(start, end), true);
        while (it.hasNext() && assembly.size() < 4000) assembly.add(line(it.next()));
        row.add("assembly", assembly);
    }

    private void disp(JsonObject row, long value, Address start, Address end) throws Exception {
        JsonArray hits = new JsonArray();
        InstructionIterator it = currentProgram.getListing().getInstructions(new AddressSet(start, end), true);
        while (it.hasNext() && hits.size() < 3000) {
            Instruction i = it.next();
            for (int op = 0; op < i.getNumOperands(); op++) {
                if ((i.getOperandType(op) & OperandType.DYNAMIC) == 0) continue;
                boolean match = false;
                for (Object o : i.getOpObjects(op))
                    if (o instanceof Scalar && ((Scalar) o).getUnsignedValue() == value) match = true;
                if (!match) continue;
                JsonObject o = new JsonObject();
                o.addProperty("instruction", line(i));
                o.addProperty("operand", op);
                Function f = getFunctionContaining(i.getAddress());
                if (f != null) o.addProperty("function", f.getEntryPoint().toString());
                hits.add(o);
            }
        }
        row.add("hits", hits);
    }
}
