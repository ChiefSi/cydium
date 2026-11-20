# Walkers: an original scene with Lemmings-style mechanics. Hardware-free, so
# the engine, generator and autopilot run in the host tests (src/test).
idf_component_register(
    SRCS "art.cpp" "autopilot.cpp" "generator.cpp" "terrain.cpp" "walkers.cpp" "world.cpp"
    INCLUDE_DIRS "include"
    REQUIRES gfx sim)
