#ifndef PICOCALC_PET_VICE_MACHINE_BUS_H
#define PICOCALC_PET_VICE_MACHINE_BUS_H

static inline int machine_bus_device_attach(unsigned int unit, const char *name,
                                            void *read_func, void *write_func,
                                            void *open_func, void *close_func,
                                            void *flush_func, void *listen_func)
{
    (void)unit;
    (void)name;
    (void)read_func;
    (void)write_func;
    (void)open_func;
    (void)close_func;
    (void)flush_func;
    (void)listen_func;
    return 0;
}

#endif
