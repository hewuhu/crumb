import crumb;

// Executable entry point; all commands live in crumb.cli.
int main(int argc, char* argv[]) {
  return crumb::CliRun(argc, argv);
}
