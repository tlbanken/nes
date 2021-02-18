target_sources(nes PRIVATE
    apu.c
    cart.c
    cpu.c
    mem.c
    nes.c
    ppu.c
    utils.c
    vac.c
)

add_subdirectory(mappers)