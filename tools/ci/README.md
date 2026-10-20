# Continuous integration

`tests.yml` is the GitHub Actions workflow that builds the editor and runs every test on a self-hosted runner: this PC, since a hosted runner can't hold Unreal Engine. It isn't in `.github/workflows/` yet, because a workflow that wants a runner nobody has set up leaves every push and pull request with a check that never finishes. Set the runner up, then move the file there.

## Setting up the runner (once, by hand)

1. On GitHub, open the repository's Settings, Actions, Runners, and choose New self-hosted runner, Windows. It shows a download link and a registration token.
2. In PowerShell, make `C:\actions-runner`, download and extract the runner there as the page says, and run `.\config.cmd --url https://github.com/golgirihm/cheesestrike --token <the token> --name Hamid-Spaceship --labels cheesestrike --unattended`. The labels must include `cheesestrike`, which the workflow asks for.
3. Run `.\run.cmd` to start it, or install it as a service from an administrator prompt (`.\svc.cmd install` then `.\svc.cmd start`) so that it survives a restart. It must run as the user whose SSH key reaches the private assets repository, since the workflow fetches the submodule over SSH.
4. Move `tools/ci/tests.yml` to `.github/workflows/tests.yml` in a pull request. Once a run has passed, require its check (`tests`) in the `Protect main` ruleset under the repository's Settings, Rules.

A run builds from a fresh checkout (a few minutes with the shared derived-data cache) and then runs the tests (about five minutes). It fails if the editor is open on this PC while it runs, since the editor holds the game module.
