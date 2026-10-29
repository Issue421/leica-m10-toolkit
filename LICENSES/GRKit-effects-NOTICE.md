# Effect attribution

Each film effect is a fit of one film simulation LUT to the effect model of the GR cameras: a
3 x 3 look matrix, optional hue sectors, three tone curves, and an optional post-curve matrix.
GRKit contributors made the fits. The repository and the SD packages contain the fitted
parameters, not the LUTs.

## spektrafilm

Portra 400, Gold 200, Ektar 100, Pro 400H, Superia 400, Vision3 250D, Vision3 500T,
Portra 800 +2, Kodachrome 64, Velvia 100, and Ektachrome E100 derive from LUTs of
[spektrafilm](https://github.com/andreavolpato/spektrafilm) 0.3.4 by Andrea Volpato,
licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
These effect definitions are also licensed under CC BY-SA 4.0.

[SPEKTRAFILM-LICENSE.txt](SPEKTRAFILM-LICENSE.txt) contains the upstream notice. The upstream changes are: GRKit scans the three slide films directly instead of printing them, sets the scan
levels of Vision3 500T, and fits each LUT to the effect model.


Only Velvia 100, Ektachrome E100 and Kodachrome 64 fit definitions are included here. Other effects, icons and fonts are not distributed. Names identify simulated materials; the models are not measured physical equivalents. These effects require the exact GRKit gamma and operate in a scene-linear camera-oriented model, not a generic display-sRGB JPEG domain.
