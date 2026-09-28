#include <telkin/Print.h>
#include <propelpartsu/PropelPartsU.h>

red::Registrar* propelpartsu::getRegistrar() {
    static red::Registrar sRegistrar("propelpartsu");
    return &sRegistrar;
}

void main() {
    tk::println("PropelParts U - Actors by Ryguy0777");
}
