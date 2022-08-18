/*
 * Copyright 2020-2022 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



/* A helper file of utilities for Mason */
public use FileSystem;
private use List;
private use Map;
public use Subprocess;
public use MasonEnv;
public use Path;
public use TOML;
use Regex;
public use Version;

const zero = 0;
const maxInt = max(int);

/* Gets environment variables for spawn commands */
extern proc getenv(name : c_string) : c_string;
proc getEnv(name: string): string {
  var cname: c_string = name.c_str();
  var value = getenv(cname);
  return createStringWithNewBuffer(value);
}


class MasonError : Error {
  var msg:string;
  proc init(msg:string) {
    this.msg = msg;
  }
  override proc message() {
    return msg;
  }
}


/* Creates the rest of the project structure */
proc makeTargetFiles(binLoc: string, projectHome: string) {

  const target = joinPath(projectHome, 'target');
  const srcBin = joinPath(target, binLoc);
  const example = joinPath(target, 'example');

  if !isDir(target) {
    mkdir(target);
  }
  if !isDir(srcBin) {
    mkdir(srcBin);
  }
  if !isDir(example) {
    mkdir(example);
  }

  const actualTest = joinPath(projectHome,'test');
  if isDir(actualTest) {
    for dir in walkdirs(actualTest) {
      const internalDir = target+dir.replace(projectHome,"");
      if !isDir(internalDir) {
        mkdir(internalDir);
      }
    }
  }
  const test = joinPath(target, 'test');
  if(!isDir(test)) {
    mkdir(test);
  }
}


proc stripExt(toStrip: string, ext: string) : string {
  if toStrip.endsWith(ext) {
    var stripped = toStrip[..<(toStrip.size - ext.size)];
    return stripped;
  }
  else {
    return toStrip;
  }
}


/* Uses the Subprocess module to create a subprocess */
proc runCommand(cmd, quiet=false) : string throws {
  var ret : string;
  try {

    var splitCmd = cmd.split();
    var process = spawn(splitCmd, stdout=pipeStyle.pipe);

    for line in process.stdout.lines() {
      ret += line;
      if !quiet {
        write(line);
      }
    }
    process.wait();
  }
  catch {
    throw new owned MasonError("Internal mason error");
  }
  return ret;
}

/* Same as runCommand but for situations where an
   exit status is needed */
proc runWithStatus(command, quiet=false): int {

  try {
    var cmd = command.split();
    var sub = spawn(cmd, stdout=pipeStyle.pipe, stderr=pipeStyle.pipe);

    var line:string;
    if !quiet {
      while sub.stdout.readLine(line) do write(line);
      while sub.stderr.readLine(line) do write(line);
    }
    sub.wait();
    return sub.exitCode;
  }
  catch {
    return -1;
  }
}

proc runWithProcess(command, quiet=false) throws {
  try {
    var cmd = command.split();
    var process = spawn(cmd, stdout=pipeStyle.pipe, stderr=pipeStyle.pipe);

    return process;
  }
  catch {
    throw new owned MasonError("Internal mason error");
    exit(0);
  }
}

proc SPACK_ROOT : string {
  const envHome = getEnv("SPACK_ROOT");
  const default = MASON_HOME + "/spack";

  const spackRoot = if !envHome.isEmpty() then envHome else default;

  return spackRoot;
}
/*
This fetches the mason-installed spack registry only.
Users that define SPACK_ROOT to their own spack installation will use
the registry of their spack installation.
*/
proc getSpackRegistry : string {
  return MASON_HOME + "/spack-registry";
}

/* uses spawnshell and the prefix to setup Spack before
   calling the spack command. This also returns the stdout
   of the spack call.
   TODO: get to work with Subprocess */
proc getSpackResult(cmd, quiet=false) : string throws {
  var ret : string;
  try {
    var prefix = "export SPACK_ROOT=" + SPACK_ROOT +
    " && export PATH=\"$SPACK_ROOT/bin:$PATH\"" +
    " && . $SPACK_ROOT/share/spack/setup-env.sh && ";
    var splitCmd = prefix + cmd;
    var process = spawnshell(splitCmd, stdout=pipeStyle.pipe, executable="bash");

    for line in process.stdout.lines() {
      ret += line;
      if quiet == false {
        write(line);
      }
    }
    process.wait();
  }
  catch {
    throw new owned MasonError("Internal mason error");
  }
  return ret;
}


/* Sets up spack by prefixing command with spack env vars
   Only returns the exit status of the command
   TODO: get to work with Subprocess */
proc runSpackCommand(command, quiet=false) {

  var prefix = "export SPACK_ROOT=" + SPACK_ROOT +
    " && export PATH=\"$SPACK_ROOT/bin:$PATH\"" +
    " && . $SPACK_ROOT/share/spack/setup-env.sh && ";

  var cmd = (prefix + command);
  var sub = spawnshell(cmd, stdout=pipeStyle.pipe, stderr=pipeStyle.pipe, executable="bash");

  // quiet flag necessary for tests to be portable
  if !quiet {
    var line:string;
    while sub.stdout.readLine(line) {
      write(line);
    }
  }
  sub.wait();

  for line in sub.stderr.lines() {
    write(line);
  }

  return sub.exitCode;
}

const negOne = -1;
private var chplVersionInfo = createVersion(negOne, negOne, negOne);
/*
   Returns a tuple containing information about the `chpl --version`:
   (major, minor, bugFix, isMaster)
*/
proc getChapelVersionInfo(): sourceVersion {
  if chplVersionInfo.major == -1 {
    var chpMaj = chplVersion.major, chpMin = chplVersion.minor, chpUp = chplVersion.update;
    chplVersionInfo = createVersion(chpMaj, chpMin, chpUp);
  }
  return chplVersionInfo;
}

private var chplVersionStr = "";
proc getChapelVersionStr() {
  return (getChapelVersionInfo():string).replace("version ", "");
}

proc gitC(newDir, command, quiet=false) throws {
  var ret : string;
  const oldDir = here.cwd();
  here.chdir(newDir);
  ret = runCommand(command, quiet);

  here.chdir(oldDir);

  return ret;
}

proc developerMode: bool {
  const env = getEnv("CHPL_DEVELOPER");
  return env != "";
}


proc getProjectHome(cwd: string, tomlName="Mason.toml") : string throws {
  const (dirname, basename) = splitPath(cwd);
  if dirname == '/' {
    throw new owned MasonError("Mason could not find your configuration file (Mason.toml)");
  }
  const tomlFile = joinPath(cwd, tomlName);
  if exists(tomlFile) {
    return cwd;
  }
  return getProjectHome(dirname, tomlName);
}


extern "struct sys_stat_s" record chpl_stat {
  var st_mtim: chpl_timespec;
}

extern "struct timespec" record chpl_timespec {
  var tv_sec: int;
  var tv_nsec: int;
}

proc getLastModified(filename: string) : int {
  use CTypes;

  extern proc sys_stat(filename: c_string, ref chpl_stat): c_int;

  var file_buf: chpl_stat;
  var file_path = filename.c_str();

  if (sys_stat(file_path, file_buf) == 0) {
    return file_buf.st_mtim.tv_sec;
    }
  return -1;
}

proc projectModified(projectHome, projectName, binLocation) : bool {
  const binaryPath = joinPath(projectHome, "target", binLocation, projectName);
  const tomlPath = joinPath(projectHome, "Mason.toml");

  if isFile(binaryPath) {
    const binModTime = getLastModified(binaryPath);
    for file in listdir(joinPath(projectHome, "src")) {
      var srcPath = joinPath(projectHome, "src", file);
      if getLastModified(srcPath) > binModTime {
        return true;
      }
    }
    if getLastModified(tomlPath) > binModTime {
      return true;
    }
    return false;
  }
  return true;
}

/* Return 'true' for valid identifiers according to Chapel parser/spec,
   otherwise 'false' */
proc isIdentifier(name:string) {

  // Identifiers can't be empty
  if name == "" then
    return false;

  // Identifiers can't start with a digit or a $
  if name[0].isDigit() then
    return false;
  if name[0] == "$" then
    return false;

  // Check all characters are legal identifier characters
  // - lower case alphabetic
  // - upper case alphabetic
  // - digits
  // - _
  // - $
  var ok = true;
  for ch in name {
    if !(ch == "$" || ch == "_" || ch.isAlnum()) then
      ok = false;
  }
  return ok;
}


proc getMasonDependencies(sourceList: list(3*string),
                          gitList: list(4*string),
                          progName: string) {

  // Declare example to run as the main module
  var masonCompopts = " ".join(" --main-module", progName, " ");

  if sourceList.size > 0 {
    const depPath = MASON_HOME + "/src/";

    // Add dependencies to project
    for (_, name, version) in sourceList {
      var depSrc = "".join(' ',depPath, name, "-", version, '/src/', name, ".chpl");
      masonCompopts += depSrc;
    }
  }
  if gitList.size > 0 {
    const gitDepPath = MASON_HOME + '/git/';

    // Add git dependencies
    for (_, name, branch, _) in gitList {
      var gitDepSrc = ' ' + gitDepPath + name + "-" + branch + '/src/' + name + ".chpl";
      masonCompopts += gitDepSrc;
    }
  }
  return masonCompopts;
}

/* Checks to see if dependency has already been
   downloaded previously */
proc depExists(dependency: string, repo='/src/') {
  var repos = MASON_HOME + repo;
  var exists = false;
  for dir in listdir(repos) {
    if dir == dependency then
      exists = true;
  }
  return exists;
}


proc getProjectType(): string throws {
  const cwd = here.cwd();
  const projectHome = getProjectHome(cwd);
  const toParse = open(projectHome + "/Mason.toml", iomode.r);
  const tomlFile = parseToml(toParse);
  if !tomlFile.pathExists("brick.type") then
    throw new owned MasonError('Type not found in TOML file; please add a type="application" key');
  return tomlFile["brick"]!["type"]!.s;
}

/* Return parsed TOML file of name depName if it is located
   in a registry in MASON_CACHED_REGISTRY. If dependency is
   not found, throw an error. TODO: Currently does not check
   on the version. */
proc getDepToml(depName: string, depVersion: string) throws {
  const pattern = compile(depName, ignoreCase=true);

  var packages: list(string);
  var versions: list(string);
  var registries: list(string);
  var results: list(string);
  for registry in MASON_CACHED_REGISTRY {
    const searchDir = registry + "/Bricks/";

    for dir in listdir(searchDir, files=false, dirs=true) {
      const name = dir.replace("/", "");
      if pattern.search(name) {
        const ver = findLatest(searchDir + dir);
        const versionZero = createVersion(zero, zero, zero);
        if ver != versionZero {
          results.append(name + " (" + (ver:string).replace("version ", "") + ")");
          packages.append(name);
          versions.append((ver:string).replace("version ", ""));
          registries.append(registry);
        }
      }
    }
  }

  if results.size > 0 {
    const brickPath = '/'.join(registries[0], 'Bricks', packages[0], versions[0]) + '.toml';
    const openFile = openreader(brickPath);
    const toml = parseToml(openFile);

    return toml;
  } else {
    throw new owned MasonError("No TOML file in registry for " + depName);
  }
}


/* Search TOML files within a package directory to find the latest package
   version number that is supported with current Chapel version */
proc findLatest(packageDir: string): sourceVersion {
  use Path;

  var ret = createVersion(zero,zero,zero);
  const suffix = ".toml";
  const packageName = basename(packageDir);
  for manifest in listdir(packageDir, files=true, dirs=false) {
    // Check that it is a valid TOML file
    if !manifest.endsWith(suffix) {
      var warningStr = "File without '.toml' extension encountered - skipping ";
      warningStr += packageName + " " + manifest;
      stderr.writeln(warningStr);
      continue;
    }

    // Skip packages that are out of version bounds
    const chplVersion = getChapelVersionInfo();

    const manifestReader = openreader(packageDir + '/' + manifest);
    const manifestToml = parseToml(manifestReader);
    const brick = manifestToml['brick'];
    var low, high = createVersion(zero,zero,zero);
    parseChplVersion(brick, low, high);
    if chplVersion < low || chplVersion > high then continue;

    // Check that Chapel version is supported
    const end = manifest.size - suffix.size;
    var ver = createVersion(manifest[0]:int, manifest[2]:int, manifest[4]:int);
    if ver > ret then ret = ver;
  }
  return ret;
}

/* Reads the Chapel version specified by a mason project's
   TOML file and returns the min and max compatible versions */
proc parseChplVersion(brick: borrowed Toml?, inout low, inout high) {
  use Regex;

  if brick == nil {
    stderr.writeln("Error: Unable to parse manifest file");
    exit(1);
  }

  // Assert some expected fields are not nil
  if brick!['name'] == nil || brick!['version'] == nil{
    stderr.writeln("Error: Unable to parse manifest file");
    exit(1);
  }

  if brick!['chplVersion'] == nil {
    const name = brick!["name"]!.s + "-" + brick!["version"]!.s;
    stderr.writeln("Brick '", name, "' missing required 'chplVersion' field");
    exit(1);
  }

  const chplVersion = brick!["chplVersion"]!.s;
  try {
    checkChplVersion(chplVersion, low, high);
  } catch e : Error {
    const name = brick!["name"]!.s + "-" + brick!["version"]!.s;
    stderr.writeln("Invalid chplVersion in package '", name, "': ", chplVersion);
    stderr.writeln("Details: ", e.message());
    exit(1);
  }
}

/* Ensure that Chapel version is properly formatted. Returns
   a tuple of the low, high supported verisons.*/
proc checkChplVersion(chplVersion, inout low, inout high) throws {
  use Regex;
  const formatMessage = "\n\n" +
    "chplVersion format must be '<version>..<version>' or '<version>'\n" +
    "A <version> must be in one of the following formats:\n" +
    "  x.x.x\n" +
    "  x.x\n" +
    "where 'x' is a positive integer.\n";

    var versions = chplVersion.split("..");
    [v in versions] v = v.strip();

    // Expecting 1 or 2 version strings
    if versions.size > 2 || versions.size < 1 {
      throw new owned MasonError("Expecting 1 or 2 versions in chplVersion range." + formatMessage);
    } else if versions.size == 2 && (versions[0] == "" || versions[1] == "") {
      throw new owned MasonError("Unbounded chplVersion ranges are not allowed." + formatMessage);
    }

    proc parseString(ver:string): sourceVersion throws {

      // Finds 'x.x' or 'x.x.x' where x is a positive number
      const pattern = "^(\\d+\\.\\d+(\\.\\d+)?)$";
      var semver : string;
      if compile(pattern).match(ver, semver).matched == false {
        throw new owned MasonError("Invalid Chapel version format: " + ver + formatMessage);
      }
      const nums = for s in semver.split(".") do s:int;
      const v = (nums[0],nums[1], if nums.size == 3 then nums[2] else 0);

      return createVersion(v[0], v[1], v[2]);
    }

    low = parseString(versions[0]);
    if versions.size == 1 then
      high = createVersion(maxInt, maxInt, maxInt);//new sourceVersion(isParam=false, vmajor=max(int), vminor=max(int), vupdate=max(int));
    else
      high = parseString(versions[1]);
     if (low <= high) == false then
      throw new owned MasonError("Lower bound of chplVersion must be <= upper bound: " + (low:string).replace("version ", "") + " > " + (high:string).replace("version ", ""));

      // return (lo, hi);
}

/* Split pkg.0_1_0 to (pkg, 0.1.0) & viceversa */
proc splitNameVersion(ref package: string, original: bool) {
  if original {
    var res = package.split('.');
    var name = res[0];
    var version = res[1];
    version = version.replace('_', '.');
    return name + ' (' + version + ')';
  }
  else {
    package = package.replace('.', '_');
    package = package.replace(' (', '.');
    package = package.replace(')', '');
    return package;
  }
}

/* Print a TOML file. Expects full path. */
proc showToml(tomlFile : string) {
  const openFile = openreader(tomlFile);
  const toml = parseToml(openFile);
  writeln(toml);
  openFile.close();
}

/* Iterator to collect fields from a toml
   TODO custom fields returned */
iter allFields(tomlTbl: Toml) {
  for (k,v) in tomlTbl.A.items() {
    if v!.tag == fieldtag.fieldToml then
      continue;
    else yield(k,v);
  }
}
