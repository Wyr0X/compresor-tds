# Ejecutar
### Descomprimir
```sh
tdsc.exe descomprimir "C:/Program Files (x86)/Tierras del Sur/Recursos/Graficos.TDS" output_folder
```

### Comprimir
```sh
tdsc.exe comprimir "C:/Program Files (x86)/Tierras del Sur/Recursos/Graficos.TDS" input_folder
```

#### Funciona con gráficos, sonidos, interfaces, etc.

# Compilar
```sh
g++ main.cpp -o tdsc.exe -O3 -std=c++17 --static 
```