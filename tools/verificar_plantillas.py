"""Verifica que cada plantilla de Excel tenga todos los nombres que usa el programa.

Uso:  pruebas --nombres | python tools/verificar_plantillas.py
"""
import sys
from pathlib import Path

from openpyxl import load_workbook

CARPETA = Path(__file__).resolve().parent.parent / "plantillas"


def main():
    errores = 0
    lineas = [l.split() for l in sys.stdin.read().splitlines() if l.strip()]
    if not lineas:
        print("No se recibieron nombres (ejecute: pruebas --nombres | python tools/verificar_plantillas.py)")
        return 1
    for partes in lineas:
        archivo, nombres = partes[0], partes[1:]
        ruta = CARPETA / archivo
        if not ruta.exists():
            print(f"FALTA la plantilla {archivo}")
            errores += 1
            continue
        definidos = set(load_workbook(ruta).defined_names.keys())
        faltan = [n for n in nombres if n not in definidos]
        sobran = sorted(definidos - set(nombres) - {"_xlnm.Print_Titles", "_xlnm.Print_Area"})
        if faltan:
            print(f"{archivo}: faltan los nombres {faltan}")
            errores += 1
        else:
            print(f"{archivo}: OK ({len(nombres)} nombres)" + (f"  (sin usar: {sobran})" if sobran else ""))
    return 1 if errores else 0


if __name__ == "__main__":
    sys.exit(main())
