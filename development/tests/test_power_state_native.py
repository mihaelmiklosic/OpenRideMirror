"""Compila y corre las comprobaciones nativas de la lógica de energía.

OrmPowerState.h decide cuándo el receptor puede dormir. Equivocarse ahí apaga
la pantalla en medio de una salida, o deja la placa drenando la batería en el
garaje — y no se puede probar en la placa hasta que llegue.

El header no incluye Arduino ni ESP-IDF a propósito, así que se compila con el
g++ del sistema y se ejercita acá. Si algún día deja de compilar de forma
nativa, este test falla, que es la señal de que alguien le metió una dependencia
del firmware y lo volvió imposible de probar.
"""
from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from openridemirror_tools.paths import repo_root

SOURCE = repo_root() / "development" / "tests" / "native" / "test_power_state.cpp"
HEADERS = repo_root() / "firmware" / "esp32" / "OpenRideMirror"


class PowerStateNativeTests(unittest.TestCase):
    def test_power_state_checks_pass(self) -> None:
        compiler = shutil.which("g++") or shutil.which("c++")
        if compiler is None:
            self.skipTest("no hay compilador de C++ disponible")

        with tempfile.TemporaryDirectory() as work:
            binary = Path(work) / "power_state_test"
            build = subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(HEADERS), "-o", str(binary), str(SOURCE)],
                capture_output=True, text=True)
            self.assertEqual(build.returncode, 0,
                             f"no compila de forma nativa:\n{build.stderr}")

            run = subprocess.run([str(binary)], capture_output=True, text=True,
                                 timeout=30)
            self.assertEqual(run.returncode, 0,
                             f"comprobaciones fallidas:\n{run.stdout}{run.stderr}")
            self.assertIn("OK:", run.stdout)


if __name__ == "__main__":
    unittest.main()
