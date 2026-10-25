import crumb.editor.cli;

// Executable entry point; all commands live in crumb.editor.cli.
int main(int argc, char* argv[]) {
  return crumb::CliRun(argc, argv);
}
