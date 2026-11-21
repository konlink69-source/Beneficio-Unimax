// Identificadores de recursos (ventanas, controles y menús).
#pragma once

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#define IDI_APP 1
#define IDR_MENU 100

// ---------------------------------------------------------------- Plantillas de diálogo
#define IDD_PAG_FACTURA 200
#define IDD_PAG_HISTORIAL 201
#define IDD_PAG_CLIENTES 202
#define IDD_PAG_PRECIOS 203
#define IDD_PAG_DEUDAS 204
#define IDD_PAG_CAJA 205
#define IDD_PAG_VENTAS 206
#define IDD_PAG_REPORTES 207
#define IDD_PAG_CONFIG 208

#define IDD_SEL_CLIENTE 220
#define IDD_NUEVA_DEUDA 221
#define IDD_DETALLE_DEUDA 222
#define IDD_ABONO 223
#define IDD_MOVIMIENTO 224
#define IDD_TEXTO 225
#define IDD_RECORDATORIO 226

// ---------------------------------------------------------------- Menú
#define IDM_RESPALDAR 40001
#define IDM_CARPETA 40002
#define IDM_SALIR 40003
#define IDM_RECORDATORIO 40004
#define IDM_ACERCA 40005
#define IDM_PLANTILLAS 40006

// ---------------------------------------------------------------- Comprobante de compra
#define IDC_F_DNI 1001
#define IDC_F_BUSCAR 1002
#define IDC_F_NOMBRE 1003
#define IDC_F_TELEFONO 1004
#define IDC_F_INFO 1005
#define IDC_F_NUMERO 1006
#define IDC_F_FECHA 1007
#define IDC_F_PRECIOS 1008
#define IDC_F_TIPO 1009
#define IDC_F_CANTIDAD 1010
#define IDC_F_UNIDAD 1011
#define IDC_F_GALONES 1012
#define IDC_F_PRECIO 1013
#define IDC_F_SUBTOTAL 1014
#define IDC_F_AGREGAR 1015
#define IDC_F_QUITAR 1016
#define IDC_F_LINEAS 1017
#define IDC_F_TOTAL 1018
#define IDC_F_PAGADO 1019
#define IDC_F_SALDO 1020
#define IDC_F_VENCE 1021
#define IDC_F_OBS 1022
#define IDC_F_GUARDAR_IMP 1023
#define IDC_F_GUARDAR 1024
#define IDC_F_LIMPIAR 1025
#define IDC_F_PESO_INFO 1026

// ---------------------------------------------------------------- Historial
#define IDC_H_DESDE 1101
#define IDC_H_HASTA 1102
#define IDC_H_FILTRO 1103
#define IDC_H_BUSCAR 1104
#define IDC_H_LISTA 1105
#define IDC_H_IMPRIMIR 1106
#define IDC_H_ANULAR 1107
#define IDC_H_RESUMEN 1108
#define IDC_H_HOY 1109
#define IDC_H_MES 1110

// ---------------------------------------------------------------- Clientes
#define IDC_C_NOMBRE 1201
#define IDC_C_DNI 1202
#define IDC_C_TELEFONO 1203
#define IDC_C_DIRECCION 1204
#define IDC_C_TIPO 1205
#define IDC_C_NUEVO 1206
#define IDC_C_GUARDAR 1207
#define IDC_C_ELIMINAR 1208
#define IDC_C_ESTADO 1209
#define IDC_C_BUSCAR 1210
#define IDC_C_LISTA 1211
#define IDC_C_RESUMEN 1212
#define IDC_C_MODO 1213

// ---------------------------------------------------------------- Precios
#define IDC_P_FECHA 1301
#define IDC_P_GUARDAR 1302
#define IDC_P_ESTADO 1303
#define IDC_P_LISTA 1304
#define IDC_P_LB1 1311
#define IDC_P_LB2 1312
#define IDC_P_LB3 1313
#define IDC_P_QQ1 1321
#define IDC_P_QQ2 1322
#define IDC_P_QQ3 1323

// ---------------------------------------------------------------- Deudas
#define IDC_D_TIPO 1401
#define IDC_D_ESTADO 1402
#define IDC_D_FILTRO 1403
#define IDC_D_BUSCAR 1404
#define IDC_D_NUEVA 1405
#define IDC_D_ABONO 1406
#define IDC_D_DETALLE 1407
#define IDC_D_ANULAR 1408
#define IDC_D_RECORDATORIO 1409
#define IDC_D_REPORTE 1410
#define IDC_D_RESUMEN 1411
#define IDC_D_LISTA 1412

// ---------------------------------------------------------------- Caja
#define IDC_K_FECHA 1501
#define IDC_K_ESTADO 1502
#define IDC_K_MOV 1503
#define IDC_K_ELIMINAR 1504
#define IDC_K_REPORTE 1505
#define IDC_K_INICIAL 1506
#define IDC_K_INGRESOS 1507
#define IDC_K_EGRESOS 1508
#define IDC_K_ESPERADO 1509
#define IDC_K_CONTADO 1510
#define IDC_K_DIFERENCIA 1511
#define IDC_K_NOTAS 1512
#define IDC_K_CERRAR 1513
#define IDC_K_REABRIR 1514
#define IDC_K_AVISO 1515
#define IDC_K_LISTA 1516
#define IDC_K_HOY 1517

// ---------------------------------------------------------------- Ventas
#define IDC_V_DNI 1601
#define IDC_V_BUSCAR 1602
#define IDC_V_NOMBRE 1603
#define IDC_V_FECHA 1604
#define IDC_V_TIPO 1605
#define IDC_V_CANTIDAD 1606
#define IDC_V_UNIDAD 1607
#define IDC_V_PRECIO 1608
#define IDC_V_TOTAL 1609
#define IDC_V_RECIBIDO 1610
#define IDC_V_VENCE 1611
#define IDC_V_OBS 1612
#define IDC_V_GUARDAR 1613
#define IDC_V_LIMPIAR 1614
#define IDC_V_DESDE 1615
#define IDC_V_HASTA 1616
#define IDC_V_LISTAR 1617
#define IDC_V_ANULAR 1618
#define IDC_V_RESUMEN 1619
#define IDC_V_LISTA 1620
#define IDC_V_NUMERO 1621
#define IDC_V_INFO 1622

// ---------------------------------------------------------------- Reportes
#define IDC_R_DESDE 1701
#define IDC_R_HASTA 1702
#define IDC_R_HOY 1703
#define IDC_R_MES 1704
#define IDC_R_COMPRAS 1705
#define IDC_R_VENTAS 1706
#define IDC_R_CAJA 1707
#define IDC_R_DEUDAS 1708
#define IDC_R_ESTADO 1709
#define IDC_R_CARPETA 1710
#define IDC_R_CIERRE 1711
#define IDC_R_MESANT 1712
#define IDC_R_INFO 1713

// ---------------------------------------------------------------- Configuración
#define IDC_G_EMPRESA 1801
#define IDC_G_RTN 1802
#define IDC_G_DIRECCION 1803
#define IDC_G_TELEFONO 1804
#define IDC_G_APERTURA 1805
#define IDC_G_NUMFAC 1806
#define IDC_G_NUMVEN 1807
#define IDC_G_PDF 1808
#define IDC_G_DIAS 1809
#define IDC_G_ALABRIR 1810
#define IDC_G_HORA 1811
#define IDC_G_ACTIVAR 1812
#define IDC_G_DESACTIVAR 1813
#define IDC_G_AVISO_ESTADO 1814
#define IDC_G_RUTA 1815
#define IDC_G_RESPALDAR 1816
#define IDC_G_CARPETA 1817
#define IDC_G_GUARDAR 1818
#define IDC_G_PROBAR 1819

// ---------------------------------------------------------------- Diálogos
#define IDC_S_BUSCAR 2001
#define IDC_S_LISTA 2002

#define IDC_N_DNI 2101
#define IDC_N_BUSCAR 2102
#define IDC_N_NOMBRE 2103
#define IDC_N_INFO 2104
#define IDC_N_PORCOBRAR 2105
#define IDC_N_PORPAGAR 2106
#define IDC_N_ORIGEN 2107
#define IDC_N_MONTO 2108
#define IDC_N_FECHA 2109
#define IDC_N_VENCE 2110
#define IDC_N_CONCEPTO 2111
#define IDC_N_CAJA 2112
#define IDC_N_CAJA_INFO 2113

#define IDC_DD_INFO 2201
#define IDC_DD_LISTA 2202
#define IDC_DD_ABONAR 2203
#define IDC_DD_ANULAR 2204

#define IDC_A_INFO 2301
#define IDC_A_MONTO 2302
#define IDC_A_TODO 2303
#define IDC_A_FECHA 2304
#define IDC_A_NOTA 2305
#define IDC_A_CAJA 2306

#define IDC_M_INGRESO 2401
#define IDC_M_EGRESO 2402
#define IDC_M_MONTO 2403
#define IDC_M_CONCEPTO 2404
#define IDC_M_INFO 2405

#define IDC_T_PROMPT 2501
#define IDC_T_TEXTO 2502

#define IDC_RC_RESUMEN 2601
#define IDC_RC_LISTA 2602
#define IDC_RC_IR 2603
#define IDC_RC_EXCEL 2604

// Control de pestañas de la ventana principal.
#define IDC_PESTANAS 3000
