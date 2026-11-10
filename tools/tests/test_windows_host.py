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
        root = Path(self.temp.name)
        self.w = SimpleNamespace(root=root, local=root / '.local', project=root / 'Game.uproject',
                                 versions={'mcp_url':'http://127.0.0.1:18765/mcp'},
                                 settings={'game_target':'Game'})
        self.d = dev.Dev(self.w, host='windows')

    def test_host_and_layout(self):
        with patch('dev.sys.platform', 'win32'), patch('dev.platform.machine', return_value='AMD64'):
            self.assertEqual(dev.current_host(), 'windows')
        self.assertEqual(self.d.platform, 'Win64')
        self.assertEqual(self.d.editor.name, 'UnrealEditor.exe')
        self.assertEqual(self.d.cmd.name, 'UnrealEditor-Cmd.exe')
        self.assertTrue(self.d.layout['build'].endswith('Build.bat'))
        self.assertIn('Quit', ' '.join(self.d.layout['automation']('Project.Expedition.')))
        self.assertNotIn('-TestExit', ' '.join(self.d.layout['automation']('Project.')))
        with patch.dict(os.environ, {'UE_ROOT':str(self.w.root / 'custom engine')}):
            self.assertEqual(dev.Dev(self.w, host='windows').engine, self.w.root / 'custom engine/Engine')

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
        with patch('windows_host.json_rows', return_value=[row,other]):
            self.assertEqual([p['pid'] for p in self.d.processes()], [12])
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
            with self.assertRaisesRegex(RuntimeError,'left running'): win.normal_quit(12)
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
        self.assertEqual(len([a for a in args if a.startswith('-EditorLayoutIni=')]),1)
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
        process.wait.side_effect=[subprocess.TimeoutExpired('game',30),0]
        with patch('dev.subprocess.Popen',return_value=process), patch('windows_host.normal_quit') as quit:
            with self.assertRaisesRegex(RuntimeError,'timed out'):
                self.d.run_game(['game.exe'],self.w.root/'game.log',30)
        quit.assert_called_once_with(12); process.terminate.assert_not_called()


if __name__ == '__main__': unittest.main()
