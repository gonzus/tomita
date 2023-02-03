#include <tap.h>
#include "util.h"
#include "tomita.h"

static void test_build_tomita(void) {
  Tomita* tomita = 0;
  do {
    ok(1, "=== TESTING tomita ===");

    tomita = tomita_create();
    ok(tomita != 0, "can create a Tomita");
    if (!tomita) break;
  } while (0);
  if (tomita) tomita_destroy(tomita);
}

int main (int argc, char* argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  do {
    test_build_tomita();
  } while (0);

  done_testing();
}
