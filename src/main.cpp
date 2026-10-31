import crumb.editor.cli;

// Editor executable entry point; all commands are implemented in crumb.editor.cli.
int main(int argc, char* argv[]) {
  return crumb::CliRun(argc, argv);
}
