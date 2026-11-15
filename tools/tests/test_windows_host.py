"""Windows dispatch/refusal tests; run on every host without an Unreal install."""
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import dev
import windows_host as win
from workspace import project_argument


class WindowsHostTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="win helper ")
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name).resolve()
        self.w = SimpleNamespace(root=root, local=root / '.local', project=root / 'Game.uproject',
                                 versions={'mcp_url':'http://127.0.0.1:18765/mcp'},
                                 settings={'game_target':'Game'})
        self.d = dev.Dev(self.w, host='windows')

    def test_perf_refuses_windows_before_engine_or_process_launch(self):
        self.assertTrue(self.d.perf_context.__doc__.startswith("Validate the arguments"))
        with patch.object(self.d, "require_engine") as engine, patch("dev.subprocess.Popen") as launch:
            for kwargs in ({}, {"editor": True}, {"only": "warmup"}, {"only": "measured"}, {"only": "resume", "run": "kept"}):
                with self.assertRaisesRegex(RuntimeError, "Windows perf launches are unsupported"):
                    self.d.perf(**kwargs)
        engine.assert_not_called()
        launch.assert_not_called()

    def test_perf_existing_report_starts_no_process_on_windows(self):
        with patch.object(self.d, "perf_run_folder", return_value=self.w.root), patch.object(self.d, "perf_report", return_value={"kept": True}) as report, patch("dev.subprocess.Popen") as launch:
            self.assertEqual(self.d.perf(only="report", run="kept"), {"kept": True})
        report.assert_called_once()
        launch.assert_not_called()

    def test_host_and_layout(self):
        with patch('dev.sys.platform', 'win32'), patch('dev.platform.machine', return_value='AMD64'):
            self.assertEqual(dev.current_host(), 'windows')
        self.assertEqual(self.d.platform, 'Win64')
        self.assertEqual(self.d.editor.name, 'UnrealEditor.exe')
        self.assertEqual(self.d.cmd.name, 'UnrealEditor-Cmd.exe')
        self.assertTrue(self.d.layout['build'].endswith('Build.bat'))
        self.assertIn(';SoftQuit', ' '.join(self.d.layout['automation']('Project.Expedition.')))
        self.assertNotIn('-TestExit', ' '.join(self.d.layout['automation']('Project.')))
        with patch.dict(os.environ, {'UE_ROOT':str(self.w.root / 'custom engine')}):
            self.assertEqual(dev.Dev(self.w, host='windows').engine, (self.w.root / 'custom engine').resolve() / 'Engine')

    def test_complete_case_insensitive_project_argument(self):
        project = self.w.project
        command = '"C:\\Epic Games\\UnrealEditor.exe" "' + str(project).upper().replace('/', '\\') + '" -log'
        self.assertTrue(project_argument(command, project, windows=True))
        for suffix in ('.backup', '-other'):
            self.assertFalse(project_argument(command.replace(str(project).upper().replace('/', '\\'), str(project).upper().replace('/', '\\') + suffix), project, windows=True))
        self.assertFalse(project_argument('UnrealEditor.exe -abslog=' + str(project), project, windows=True))
        self.assertFalse(project_argument('UnrealEditor.exe -abslog="' + str(project) + '"', project, windows=True))

    def test_process_scan_associates_only_exact_project(self):
        row = {'ProcessId':12,'ExecutablePath':str(self.d.editor),'CommandLine':'UnrealEditor.exe "' + str(self.w.project) + '"'}
        other = {**row, 'ProcessId':13, 'CommandLine':row['CommandLine'].replace('.uproject', '.uproject.backup')}
        with patch('windows_host.json_rows', return_value=[row]):
            self.assertEqual([p['pid'] for p in self.d.processes()], [12])
        with patch('windows_host.json_rows', return_value=[other]):
            with self.assertRaisesRegex(RuntimeError, 'ownership is unknown'): self.d.require_closed()
        with patch('windows_host.json_rows', return_value=[{**row,'CommandLine':None}]):
            with self.assertRaisesRegex(RuntimeError,'ownership is unknown'): self.d.processes()

    def test_listener_ambiguity_and_empty_query(self):
        rows = [{'OwningProcess':12,'LocalPort':18765},{'OwningProcess':13,'LocalPort':18765}]
        with patch('windows_host.json_rows', return_value=rows):
            with self.assertRaisesRegex(RuntimeError,'Multiple'): self.d.listener()
        with patch('windows_host.json_rows', return_value=[]): self.assertIsNone(self.d.listener())
        with patch('windows_host.json_rows', return_value=rows[:1]): self.assertEqual(self.d.listener(),12)

    def test_listener_must_match_exact_editor_executable(self):
        process = {'pid':12, 'executable':str(self.d.editor), 'command':'quoted editor'}
        with patch.object(self.d, 'listener', return_value=12), patch.object(self.d, 'processes', return_value=[process]):
            self.assertEqual(self.d.os_identity(),12)
        process['executable']=str(self.w.root / 'other/UnrealEditor.exe')
        with patch.object(self.d,'listener',return_value=12), patch.object(self.d,'processes',return_value=[process]):
            with self.assertRaisesRegex(RuntimeError,'not a verified'): self.d.os_identity()

    def test_normal_quit_is_window_close_and_refuses_declined_request(self):
        with patch('windows_host.powershell',return_value='True') as ps: win.normal_quit(12)
        self.assertIn('CloseMainWindow()',ps.call_args.args[0])
        self.assertNotIn('Stop-Process',ps.call_args.args[0])
        with patch('windows_host.powershell',return_value='False'):
            with self.assertRaisesRegex(RuntimeError,'declined'): win.normal_quit(12)
        with self.assertRaises(ValueError): win.normal_quit(0)

    def test_windows_pid_query_never_uses_kill(self):
        with patch('dev.sys.platform','win32'), patch('windows_host.pid_alive',return_value=False), patch('dev.os.kill') as kill:
            self.assertFalse(dev.pid_exists(12)); kill.assert_not_called()

    def test_batch_paths_with_spaces_and_refused_shell_expansion(self):
        command = win.batch_command(['C:/Epic Games/Build.bat','My Target','-project=C:/My Game/Game.uproject'])
        self.assertIn('/d /s /c ""C:/Epic Games/Build.bat" "My Target"', command)
        for bad in ('a&b','%PATH%','x|y','x\ny','x"y'):
            with self.assertRaises(ValueError): win.batch_command(['C:/Build.bat',bad])
        self.assertEqual(win.batch_command(['git','status']),['git','status'])

    def test_native_launch_priority_layout_and_live_coding(self):
        args=self.d.native_command([self.d.cmd,self.w.project])
        self.assertIn('-NoLiveCoding',args)
        self.assertEqual(args.count('-EditorLayoutIni='),1)
        self.assertEqual(self.d.native_options(),{'creationflags':win.BELOW_NORMAL})
        other=dev.Dev(self.w,host='mac')
        self.assertEqual(other.native_command(['editor','project']),['editor','project'])
        self.assertEqual(other.native_options(),{})

    def test_package_has_real_window_private_save_and_win64_binary(self):
        flags=self.d.package_launch_flags(self.w.root)
        self.assertIn('-windowed',flags)
        self.assertNotIn('-sm6',flags)  # respect the project's cooked shader targets
        self.assertNotIn('-dx12',flags)
        self.assertNotIn('-nullrhi',flags)
        self.assertIn('-ExpeditionSaveRoot=' + str(self.w.root/'save'),flags)
        binary=self.w.root/'Windows/Game/Binaries/Win64/Game.exe'
        binary.parent.mkdir(parents=True)
        binary.touch(); binary.chmod(0o755)
        self.assertEqual(self.d.packaged_binary(self.w.root),binary)

    def test_dxdiag_is_capability_probe_not_sm6_acceptance(self):
        xml='<DxDiag><SystemInformation><DirectXVersion>DirectX 12</DirectXVersion></SystemInformation><DisplayDevices><DisplayDevice><CardName>GPU</CardName><FeatureLevels>12_2,12_1,12_0,11_1</FeatureLevels><DriverVersion>1</DriverVersion></DisplayDevice></DisplayDevices></DxDiag>'
        result=win.parse_dxdiag(xml)
        self.assertEqual(result['adapters'][0]['name'],'GPU')
        self.assertIn('rendered runtime log',result['note'])
        with self.assertRaises(RuntimeError): win.parse_dxdiag(xml.replace('12_2,12_1,12_0,11_1','11_1'))

    def test_windows_lock_has_one_byte_and_unlocks_on_exception(self):
        lock=Mock(LK_NBLCK=1,LK_UNLCK=2)
        with patch.dict(sys.modules, {'msvcrt':lock}):
            with self.assertRaisesRegex(ValueError,'boom'):
                with win.operation_lock(self.w.root/'op.lock'): raise ValueError('boom')
        self.assertEqual((self.w.root/'op.lock').read_bytes(),b'0')
        self.assertEqual([c.args[1] for c in lock.locking.call_args_list],[1,2])
        lock.locking.side_effect=OSError('busy')
        with patch.dict(sys.modules,{'msvcrt':lock}):
            with self.assertRaisesRegex(RuntimeError,'Another helper'):
                with win.operation_lock(self.w.root/'op.lock'): pass

    def test_game_timeout_requests_normal_close_never_terminate(self):
        process=Mock(pid=12)
        process.poll.return_value = None
        process.wait.side_effect=[subprocess.TimeoutExpired('game',30),0]
        with patch('dev.subprocess.Popen',return_value=process), patch('windows_host.normal_quit') as quit:
            with self.assertRaisesRegex(RuntimeError,'timed out'):
                self.d.run_game(['game.exe'],self.w.root/'game.log',30)
        quit.assert_called_once_with(12); process.terminate.assert_not_called()




    def test_unreal_key_values_quote_only_the_value(self):
        args = [self.d.cmd, self.w.project]
        for key in ("abslog", "ReportExportPath", "ExpeditionSaveRoot", "script"):
            args.append(f"-{key}={self.w.root / ('private ' + key)}")
        command = self.d.native_command(args)
        for arg in args[2:]:
            key, value = arg.split("=", 1)
            self.assertIn(key + '=\"' + value + '\"', command)
            self.assertNotIn('\"' + key + '=', command)
        self.assertIn('-EditorLayoutIni=\"', command)
        self.assertTrue(command.startswith('\"' + str(self.d.cmd) + '\" '))
        self.assertEqual(win.unreal_command(['Game.exe', '-nullrhi', '-NoLiveCoding', '-empty=']), 'Game.exe -nullrhi -NoLiveCoding -empty=\"\"')
        for bad in ('-script=a"b', '-script=a\nb'):
            with self.assertRaises(ValueError): win.unreal_command([self.d.cmd, bad])
        # The packaged executable is not named UnrealEditor, but needs the same parser.
        command = self.d.native_command(['Game.exe', '-ExpeditionSaveRoot=' + str(self.w.root)], unreal=True)
        self.assertIn('-ExpeditionSaveRoot=\"', command)

    def test_powershell_uses_utf8_for_unicode_process_paths(self):
        with patch('windows_host.subprocess.run', return_value=Mock(stdout='{"path":"Jos\u00e9"}')) as run:
            self.assertEqual(win.json_rows('fixed script'), [{'path':'Jos\u00e9'}])
        self.assertEqual(run.call_args.kwargs['encoding'], 'utf-8')
        self.assertIn('[Console]::OutputEncoding', run.call_args.args[0][-1])

    def test_unmatched_or_relative_editors_refuse_a_closed_host(self):
        for argument in ('Game.uproject', 'C:/SHORT~1/Game.uproject', 'Z:/Game.uproject', 'C:/other/Other.uproject'):
            row = {'ProcessId':12, 'ExecutablePath':str(self.d.editor), 'CommandLine':'UnrealEditor.exe ' + argument}
            with self.subTest(argument=argument), patch('windows_host.json_rows', return_value=[row]):
                with self.assertRaisesRegex(RuntimeError, 'ownership is unknown'): self.d.require_closed()
        project = self.w.root / 'Jos\u00e9' / 'Game.uproject'
        row['CommandLine'] = 'UnrealEditor.exe "' + str(project) + '"'
        with patch('windows_host.json_rows', return_value=[row]):
            self.assertEqual(win.editor_processes(project)[0]['pid'], 12)

    def test_windows_close_twice_clean_and_waits_without_signals(self):
        clean = {'dirty_packages':[], 'pie_active':False}
        @contextmanager
        def connected(): yield Mock(), clean
        with patch.object(self.d, 'require_engine'), patch.object(self.d, 'os_identity', return_value=12), \
             patch.object(self.d, 'connected', connected), patch.object(self.d, 'runtime', return_value=clean) as state, \
             patch.object(self.d, 'processes', return_value=[]), patch.object(self.d, 'listener', return_value=None), \
             patch('dev.pid_exists', side_effect=[True, True, False]), patch('dev.time.sleep') as wait, \
             patch('windows_host.normal_quit') as quit, patch('dev.os.kill') as kill:
            self.d.close()
        state.assert_called_once(); quit.assert_called_once_with(12)
        self.assertEqual(wait.call_count, 2); kill.assert_not_called()

    def test_windows_close_refuses_dirty_pie_changed_identity_and_request_quit(self):
        clean = {'dirty_packages':[], 'pie_active':False}
        for dirty in ({'dirty_packages':['/Temp/Unsaved'], 'pie_active':False}, {'dirty_packages':[], 'pie_active':True}):
            for second_check in (False, True):
                @contextmanager
                def connected(): yield Mock(), clean if second_check else dirty
                with patch.object(self.d, 'require_engine'), patch.object(self.d, 'os_identity', return_value=12), \
                     patch.object(self.d, 'connected', connected), patch.object(self.d, 'runtime', return_value=dirty), \
                     patch('windows_host.normal_quit') as quit:
                    with self.assertRaises(RuntimeError): self.d.close()
                    quit.assert_not_called()
        @contextmanager
        def connected(): yield Mock(), clean
        with patch.object(self.d, 'require_engine'), patch.object(self.d, 'os_identity', side_effect=[12,13]), \
             patch.object(self.d, 'connected', connected), patch.object(self.d, 'runtime', return_value=clean), \
             patch('windows_host.normal_quit') as quit:
            with self.assertRaisesRegex(RuntimeError, 'identity changed'): self.d.close()
            quit.assert_not_called()
        with patch.object(self.d, 'require_engine'), patch('windows_host.normal_quit') as quit:
            with self.assertRaisesRegex(RuntimeError, 'save dialog'): self.d.close(request_quit=True)
            quit.assert_not_called()
        with patch.object(self.d, 'listener', return_value=None), patch.object(self.d, 'processes', return_value=[]):
            with self.assertRaisesRegex(RuntimeError, 'not a verified'): self.d.os_identity()

    def test_pid_alive_distinguishes_missing_denied_exited_and_query_failure(self):
        kernel = Mock()
        kernel.OpenProcess.return_value = 0
        with patch('windows_host.ctypes.WinDLL', return_value=kernel, create=True), \
             patch('windows_host.ctypes.get_last_error', return_value=87, create=True):
            self.assertFalse(win.pid_alive(12))
        with patch('windows_host.ctypes.WinDLL', return_value=kernel, create=True), \
             patch('windows_host.ctypes.get_last_error', return_value=5, create=True):
            with self.assertRaisesRegex(RuntimeError, 'unknown'): win.pid_alive(12)
        kernel.OpenProcess.return_value = 123
        for code, expected in ((259,True), (0,False)):
            def query(handle, pointer): pointer._obj.value = code; return 1
            kernel.GetExitCodeProcess.side_effect = query
            with patch('windows_host.ctypes.WinDLL', return_value=kernel, create=True):
                self.assertEqual(win.pid_alive(12), expected)
        kernel.GetExitCodeProcess.side_effect = None
        kernel.GetExitCodeProcess.return_value = 0
        with patch('windows_host.ctypes.WinDLL', return_value=kernel, create=True):
            with self.assertRaisesRegex(RuntimeError, 'exit status'): win.pid_alive(12)
        self.assertEqual(kernel.CloseHandle.call_count, 3)

    def test_game_timeout_preserves_context_when_close_fails_or_is_ignored(self):
        for decline in (True, False):
            process = Mock(pid=12)
            process.poll.return_value = None
            process.wait.side_effect = subprocess.TimeoutExpired('game',30)
            with patch('dev.subprocess.Popen', return_value=process), \
                 patch('windows_host.normal_quit', side_effect=RuntimeError('declined') if decline else None) as quit:
                with self.assertRaisesRegex(RuntimeError, 'timed out after 30s.*left running') as caught:
                    self.d.run_game(['game.exe'], self.w.root/'game.log', 30)
            self.assertNotIn('SIGTERM', str(caught.exception))
            quit.assert_called_once_with(12); process.terminate.assert_not_called(); process.kill.assert_not_called()

    def test_package_preserves_startup_failure_when_close_fails_or_is_ignored(self):
        self.w.game_map = '/Game/Test'
        for decline in (True, False):
            process = Mock(pid=12)
            process.poll.return_value = None
            process.wait.side_effect = subprocess.TimeoutExpired('game',30)
            clock = iter([0,0,100,100])
            with patch.object(self.d, 'require_engine'), patch.object(self.d, 'require_closed'), patch.object(self.d, 'execute'), \
                 patch.object(self.d, 'packaged_binary', return_value=self.w.root/'Game.exe'), \
                 patch('dev.subprocess.Popen', return_value=process), patch('dev.time.monotonic', side_effect=lambda:next(clock)), \
                 patch('windows_host.normal_quit', side_effect=RuntimeError('declined') if decline else None) as quit:
                with self.assertRaisesRegex(RuntimeError, 'left running.*Earlier failure: Packaged game did not enter') as caught:
                    self.d.package()
            self.assertNotIn('SIGTERM', str(caught.exception))
            quit.assert_called_once_with(12); process.terminate.assert_not_called(); process.kill.assert_not_called()
            self.assertIsInstance(caught.exception.__cause__, RuntimeError)

    def test_windows_build_automation_bootstrap_commands_and_doctor(self):
        import io
        self.w.local.mkdir()
        self.w.settings['editor_target'] = 'GameEditor'
        self.w.project.write_text('{}')
        with patch.object(self.d, 'require_engine'), patch.object(self.d, 'require_closed'), \
             patch.object(self.d, 'build_identity', return_value={}), patch.object(self.d, 'run_logged', return_value=0) as run:
            self.d.build()
        command = run.call_args.args[0]
        self.assertEqual(command[1:4], ['GameEditor','Win64','Development'])
        self.assertIn('/d /s /c', self.d.native_command(command))
        command = self.d.native_command(self.d.automation_command('Project.', self.w.root/'report path', self.w.root/'a.log'))
        for flag in ('-nullrhi','-NoLiveCoding',';SoftQuit','-ReportExportPath=\"'): self.assertIn(flag, command)
        def bounded(args, log, timeout):
            Path(next(str(a).split('=',1)[1] for a in args if str(a).startswith('-abslog='))).write_text('bootstrap')
            return 0
        with patch.object(self.d, 'require_engine'), patch.object(self.d, 'require_closed'), \
             patch('dev.validate_bootstrap'), patch.object(self.d, 'run_bounded', side_effect=bounded) as run:
            self.d.bootstrap()
        command = self.d.native_command(run.call_args.args[0])
        for flag in ('-run=pythonscript','-script=\"','-nullrhi','-NoLiveCoding'): self.assertIn(flag, command)
        with patch('sys.stdout', io.StringIO()), patch.object(self.d, 'tool', return_value='/tool'), \
             patch.object(self.d, 'require_engine'), patch.object(self.d, 'check_plugins'), patch.object(self.d, 'check_configs'), \
             patch.object(self.d, 'execute', return_value=Mock(stdout='git-lfs')), patch('windows_host.dx12_report', return_value={}) as dx:
            self.d.doctor()
        dx.assert_called_once_with(self.w.local/'doctor/dxdiag.xml')
        self.assertEqual(self.d.gallery_flags(), [])
        for host in ('linux','mac'):
            other = dev.Dev(self.w, host=host)
            self.assertEqual(other.gallery_flags(), other.layout['headless'])


    def test_debuggame_editor_is_scanned_and_refused(self):
        row = {'ProcessId':12, 'ExecutablePath':str(self.d.editor.with_name('UnrealEditor-Win64-DebugGame.exe')),
               'CommandLine':'UnrealEditor-Win64-DebugGame.exe "' + str(self.w.project) + '"'}
        with patch('windows_host.json_rows', return_value=[row]) as rows:
            with self.assertRaisesRegex(RuntimeError, 'project is open'): self.d.require_closed()
        self.assertIn("Name LIKE 'UnrealEditor%'", rows.call_args.args[0])

    def test_powershell_timeout_is_a_refusal_not_a_traceback(self):
        with patch('windows_host.subprocess.run', side_effect=subprocess.TimeoutExpired('powershell',30)):
            with self.assertRaisesRegex(RuntimeError, 'host inspection failed'): win.powershell('fixed script')

    def test_dxdiag_never_reuses_old_xml_and_reports_parse_or_timeout_failure(self):
        old = self.w.root/'dxdiag.xml'
        old.write_text('old report')
        def probe(args, **kwargs): Path(args[-1]).write_text('<bad')
        with patch('windows_host.subprocess.run', side_effect=probe):
            with self.assertRaisesRegex(RuntimeError, 'malformed'): win.dx12_report(old)
        self.assertEqual(old.read_text(), 'old report')
        with patch('windows_host.subprocess.run', return_value=Mock()):
            with self.assertRaises(FileNotFoundError): win.dx12_report(old)
        with patch('windows_host.subprocess.run', side_effect=subprocess.TimeoutExpired('dxdiag',90)):
            with self.assertRaisesRegex(RuntimeError, 'probe failed'): win.dx12_report(old)

    def test_game_timeout_race_reports_an_exited_process_without_requesting_quit(self):
        process = Mock(pid=12)
        process.poll.return_value = 0
        process.wait.side_effect = subprocess.TimeoutExpired('game',30)
        with patch('dev.subprocess.Popen', return_value=process), patch('windows_host.normal_quit') as quit:
            with self.assertRaisesRegex(RuntimeError, 'exited before the close request'):
                self.d.run_game(['game.exe'], self.w.root/'game.log',30)
        quit.assert_not_called(); process.terminate.assert_not_called()


if __name__ == '__main__': unittest.main()
