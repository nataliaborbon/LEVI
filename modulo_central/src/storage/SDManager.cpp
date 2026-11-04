#include "storage/SDManager.h"

bool initSD() {
    SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
    if (!SD_MMC.begin(SD_MOUNT_POINT, true, false, SDMMC_FREQ_DEFAULT)) {
        Serial.println("[SD] Error: no se pudo montar la tarjeta SD.");
        return false;
    }

    uint8_t tipo = SD_MMC.cardType();
    const char * tipoStr = (tipo == CARD_MMC)    ? "MMC"
                           : (tipo == CARD_SD)   ? "SDSC"
                           : (tipo == CARD_SDHC) ? "SDHC"
                                                 : "DESCONOCIDO";
    Serial.printf("[SD] Tarjeta SD montada correctamente (SD_MMC 1-bit). Tipo: %s, Tamano: %llu MB\n", tipoStr,
                  SD_MMC.cardSize() / (1024ULL * 1024ULL));
    return true;
}

bool writeFile(fs::FS & fs, const char * path, const char * data) {
    File file = fs.open(path, FILE_WRITE);
    if (!file)
        return false;

    bool ok = file.print(data);
    file.close();
    return ok;
}
