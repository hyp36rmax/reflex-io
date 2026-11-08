#include "platform_maintenance_runtime.h"
#include "../../product_config.h"
#include "../../core/button_remap.h"
#include "../../core/configuration_persistence.h"
#include "../../core/settings_store.h"
#include "platform_maintenance_product_services.h"

void servicePlatformRuntimeMaintenanceHealth() {
  productRuntimeMaintenanceHealth();
}

bool servicePlatformRuntimeExclusiveMaintenance() {
  if (productRuntimeExclusiveMaintenance()) return true;
  configurationPersistenceService();
  // An open remap editor keeps its draft; the pair reloads after it closes.
  if (!remapMenu.isOpen()) refreshRemapsForOutputChange();
  return false;
}
