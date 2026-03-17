// SilverOS JS Shell
println("=================================");
println("  SilverOS v0.1.0 — JS Shell");
println("=================================");
println("");

while (true) {
    print("js> ");
    let line = "";
    while (true) {
        let ch = readChar();
        if (ch === 10 || ch === 13) { // Newline
            println("");
            break;
        }
        if (ch === 8) { // Backspace
            if (line.length > 0) {
                line = line.substring(0, line.length - 1);
                print("\b \b");
            }
            continue;
        }
        if (ch >= 32 && ch < 127) {
            let s = String.fromCharCode(ch);
            line += s;
            print(s);
        }
    }
    
    let cmd = line.trim();
    if (cmd === "help") {
        println("JS Shell commands: help, version, clear, exit");
    } else if (cmd === "version") {
        println(os.version());
    } else if (cmd === "clear") {
        // TODO: Implement clear() binding
        println("Clear command not yet implemented.");
    } else if (cmd === "exit") {
        println("Exiting JS Shell...");
        break;
    } else if (cmd !== "") {
        println("Unknown JS command: " + cmd);
    }
}
