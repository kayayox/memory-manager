# Memory Manager

Un gestor de memoria dinámica en C con múltiples estrategias de asignación.

## Características

- ✅ Múltiples estrategias de asignación (First Fit, Best Fit, Worst Fit, Next Fit)
- ✅ Gestión de clientes múltiples
- ✅ Métricas y estadísticas en tiempo real
- ✅ Detección de corrupción de memoria
- ✅ Sistema de logging extensivo
- ✅ Thread-safe con pthreads

## Estructura del Proyecto

```
memory-manager/
├── include/          # Headers públicos
├── src/             # Código fuente
├── examples/        # Ejemplos de uso
├── tests/           # Pruebas unitarias
└── build/           # Archivos de compilación
```

## Compilación

```bash
# Compilación de desarrollo
make debug

# Compilación de release
make release

# Ejecutar ejemplo básico
./build/basic_usage
```

## Próximas Implementaciones

- [ ] Estrategia Worst Fit
- [ ] Estrategia Next Fit  
- [ ] Pruebas de estrés
- [ ] Optimización de rendimiento
- [ ] Documentación completa
