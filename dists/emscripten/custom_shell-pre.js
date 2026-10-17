/*global Module*/
Module["arguments"] = [];

// Add all parameters passed via the fragment identifier
if (window.location.hash.length > 0) {
    params = decodeURI(window.location.hash.substring(1)).split(" ")
    params.forEach((param) => {
        Module["arguments"].push(param);
    })
}

// Local games: let the user pick game folders from their computer. The files
// are copied into /games, which is persisted in IndexedDB, so nothing is
// uploaded anywhere. ScummVM only starts once the user leaves the picker.
// Skipped when arguments are passed via the fragment identifier.
var GAMES_PATH = "/games";

function gamesSync(populate) {
	return new Promise((resolve, reject) => {
		FS.syncfs(populate, (err) => err ? reject(err) : resolve());
	});
}

function gamesList() {
	return FS.readdir(GAMES_PATH).filter((name) => name !== "." && name !== ".." &&
		FS.isDir(FS.stat(GAMES_PATH + "/" + name).mode)).sort();
}

function gamesRemoveTree(path) {
	FS.readdir(path).forEach((name) => {
		if (name === "." || name === "..")
			return;
		var child = path + "/" + name;
		if (FS.isDir(FS.stat(child).mode))
			gamesRemoveTree(child);
		else
			FS.unlink(child);
	});
	FS.rmdir(path);
}

async function gamesImport(files, setProgress) {
	// webkitRelativePath is "<picked folder>/<sub/dirs>/<file>"
	var added = new Set();
	for (var i = 0; i < files.length; i++) {
		var file = files[i];
		var relPath = file.webkitRelativePath || file.name;
		if (relPath.split("/").some((part) => part.startsWith(".")))
			continue;
		var target = GAMES_PATH + "/" + relPath;
		FS.mkdirTree(target.substring(0, target.lastIndexOf("/")));
		FS.writeFile(target, new Uint8Array(await file.arrayBuffer()));
		added.add(relPath.split("/")[0]);
		setProgress("Copying " + (i + 1) + "/" + files.length + ": " + relPath);
	}
	setProgress("Saving to browser storage…");
	await gamesSync(false);
	return added;
}

// The bundled demos are the targets in the shipped scummvm.ini whose path is
// under /data/games. Starting one by target name applies its learn options.
async function demosList() {
	var response = await fetch("scummvm.ini");
	if (!response.ok)
		return [];
	var demos = [], current = null;
	(await response.text()).split(/\r?\n/).forEach((line) => {
		var section = line.match(/^\[(.+)\]$/);
		if (section) {
			current = { target: section[1] };
			demos.push(current);
			return;
		}
		var pair = line.match(/^([^=]+)=(.*)$/);
		if (current && pair)
			current[pair[1]] = pair[2];
	});
	return demos.filter((demo) => demo.path && demo.path.startsWith("/data/games/"));
}

function gamesPicker(demos) {
	var panel = document.getElementById("games-panel");
	var demoList = document.getElementById("demos-list");
	var list = document.getElementById("games-list");
	var input = document.getElementById("games-input");
	var progress = document.getElementById("games-progress");

	function setProgress(text) {
		progress.hidden = !text;
		progress.textContent = text || "";
	}

	function start(args) {
		args.forEach((arg) => Module["arguments"].push(arg));
		panel.hidden = true;
		removeRunDependency("games-picker");
	}

	async function busy(fn) {
		panel.classList.add("busy");
		try {
			await fn();
		} catch (err) {
			console.error(err);
			setProgress("Error: " + err);
			return;
		} finally {
			panel.classList.remove("busy");
		}
		setProgress("");
	}

	function renderDemos() {
		demos.forEach((demo) => {
			var item = document.createElement("li");
			var button = document.createElement("button");
			// "Freddi Fish 1: … (Demo/Windows/English)" -> "Freddi Fish 1: …"
			var name = (demo.description || demo.target).replace(/\s*\([^)]*\)\s*$/, "");
			button.setAttribute("aria-label", "Play the " + name + " demo");
			// Artwork from scummvm-icons, shipped as assets/demo-<gameid>.png
			var art = document.createElement("img");
			art.src = "demo-" + demo.gameid + ".png";
			art.alt = "";
			art.onerror = () => art.remove();
			var ribbon = document.createElement("span");
			ribbon.className = "ribbon";
			ribbon.textContent = "Demo";
			var title = document.createElement("span");
			title.className = "title";
			title.textContent = name;
			button.append(art, ribbon, title);
			button.onclick = () => start([demo.target]);
			item.appendChild(button);
			demoList.appendChild(item);
		});
	}

	function render() {
		list.textContent = "";
		var games = gamesList();
		list.hidden = games.length === 0;
		games.forEach((name) => {
			var item = document.createElement("li");
			var label = document.createElement("span");
			label.textContent = GAMES_PATH + "/" + name;
			var play = document.createElement("button");
			play.textContent = "Play";
			play.onclick = () => start(["--path=" + GAMES_PATH + "/" + name, "--auto-detect"]);
			var remove = document.createElement("button");
			remove.textContent = "Remove";
			remove.onclick = () => busy(async () => {
				gamesRemoveTree(GAMES_PATH + "/" + name);
				await gamesSync(false);
				render();
			});
			item.append(label, play, remove);
			list.appendChild(item);
		});
	}

	input.onchange = () => busy(async () => {
		var files = Array.from(input.files);
		input.value = "";
		var added = await gamesImport(files, setProgress);
		render();
		// Start the launcher and have it detect the new game, which opens its
		// options dialog (read by OSystem_Emscripten::initBackend)
		if (added.size > 0) {
			ENV["SCUMMVM_ADD_GAME"] = GAMES_PATH + "/" + added.values().next().value;
			start([]);
		}
	});
	document.getElementById("games-launcher").onclick = () => start([]);

	renderDemos();
	render();
	panel.hidden = false;
	Module.setStatus("");
}

if (Module["arguments"].length === 0) {
	Module["preRun"].push(() => {
		addRunDependency("games-picker");
		FS.mkdir(GAMES_PATH);
		FS.mount(IDBFS, {}, GAMES_PATH);
		Promise.all([
			gamesSync(true).catch((err) => console.error("Could not load stored games:", err)),
			demosList().catch((err) => { console.error("Could not load demos:", err); return []; })
		]).then(([, demos]) => gamesPicker(demos));
	});
}

// MIDI support
var midiOutputMap;
if (!("requestMIDIAccess" in navigator)) {
	console.error("No MIDI support in your browser.");
} else {
	navigator
		.requestMIDIAccess({ sysex: true, software: true })
		.then((midiAccess) => {
			midiOutputMap = midiAccess.outputs;
			midiAccess.onstatechange = (e) => {
				midiOutputMap = e.target.outputs;
			};
		});
}
