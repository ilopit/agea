"""Download Khronos glTF-Sample-Assets .glb files for converter tests.

Called from CMakeLists.txt during configure via execute_process.
Usage: python tools/download_test_assets.py --output <dir> [--force]
"""

import argparse
import os
import sys
import urllib.request
import urllib.error

BASE_URL = (
    "https://raw.githubusercontent.com/KhronosGroup/"
    "glTF-Sample-Assets/main/Models/{model}/glTF-Binary/{model}.glb"
)

MODELS = [
    "ABeautifulGame",
    "AlphaBlendModeTest",
    "AnimatedColorsCube",
    "AnimatedMorphCube",
    "AnimationPointerUVs",
    "AnisotropyBarnLamp",
    "AnisotropyDiscTest",
    "AnisotropyRotationTest",
    "AnisotropyStrengthTest",
    "AntiqueCamera",
    "AttenuationTest",
    "Avocado",
    "BarramundiFish",
    "BoomBox",
    "Box",
    "BoxAnimated",
    "BoxInterleaved",
    "BoxTextured",
    "BoxTexturedNonPowerOfTwo",
    "BoxVertexColors",
    "BrainStem",
    "CarConcept",
    "CarbonFibre",
    "CesiumMan",
    "CesiumMilkTruck",
    "ChairDamaskPurplegold",
    "ChronographWatch",
    "ClearCoatCarPaint",
    "ClearCoatTest",
    "ClearcoatWicker",
    "CommercialRefrigerator",
    "CompareAlphaCoverage",
    "CompareAmbientOcclusion",
    "CompareAnisotropy",
    "CompareBaseColor",
    "CompareClearcoat",
    "CompareDispersion",
    "CompareEmissiveStrength",
    "CompareIor",
    "CompareIridescence",
    "CompareMetallic",
    "CompareNormal",
    "CompareRoughness",
    "CompareSheen",
    "CompareSpecular",
    "CompareTransmission",
    "CompareVolume",
    "Corset",
    "CubeVisibility",
    "DamagedHelmet",
    "DiffuseTransmissionPlant",
    "DiffuseTransmissionTeacup",
    "DiffuseTransmissionTest",
    "DirectionalLight",
    "DispersionTest",
    "DragonAttenuation",
    "DragonDispersion",
    "Duck",
    "EmissiveStrengthTest",
    "Fox",
    "GlamVelvetSofa",
    "GlassBrokenWindow",
    "GlassHurricaneCandleHolder",
    "GlassVaseFlowers",
    "IORTestGrid",
    "InterpolationTest",
    "IridescenceAbalone",
    "IridescenceLamp",
    "IridescenceSuzanne",
    "IridescentDishWithOlives",
    "Lantern",
    "LightVisibility",
    "LightsPunctualLamp",
    "MaterialsVariantsShoe",
    "MetalRoughSpheres",
    "MetalRoughSpheresNoTextures",
    "MorphPrimitivesTest",
    "MorphStressTest",
    "MosquitoInAmber",
    "MultiUVTest",
    "NegativeScaleTest",
    "NodePerformanceTest",
    "NormalTangentMirrorTest",
    "NormalTangentTest",
    "OrientationTest",
    "PlaysetLightTest",
    "PointLightIntensityTest",
    "PotOfCoals",
    "PotOfCoalsAnimationPointer",
    "RecursiveSkeletons",
    "RiggedFigure",
    "RiggedSimple",
    "ScatteringSkull",
    "SheenChair",
    "SheenTestGrid",
    "SheenWoodLeatherSofa",
    "SimpleInstancing",
    "SpecGlossVsMetalRough",
    "SpecularSilkPouf",
    "SpecularTest",
    "SunglassesKhronos",
    "TextureCoordinateTest",
    "TextureEncodingTest",
    "TextureLinearInterpolationTest",
    "TextureSettingsTest",
    "TextureTransformMultiTest",
    "ToyCar",
    "TransmissionOrderTest",
    "TransmissionRoughnessTest",
    "TransmissionTest",
    "TransmissionThinwallTestGrid",
    "USDShaderBallForGltf",
    "UnlitTest",
    "VertexColorTest",
    "VirtualCity",
    "WaterBottle",
    "XmpMetadataRoundedCube",
]


def download_file(url, dest):
    try:
        urllib.request.urlretrieve(url, dest)
        return True
    except (urllib.error.HTTPError, urllib.error.URLError):
        if os.path.exists(dest):
            os.remove(dest)
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="Output directory")
    parser.add_argument("--force", action="store_true", help="Re-download existing files")
    args = parser.parse_args()

    os.makedirs(args.output, exist_ok=True)

    downloaded = 0
    skipped = 0
    failed = 0

    print(f"Downloading Khronos glTF test assets ({len(MODELS)} models)...")

    for model in MODELS:
        dest = os.path.join(args.output, f"{model}.glb")

        if os.path.exists(dest) and not args.force:
            skipped += 1
            continue

        url = BASE_URL.format(model=model)
        if download_file(url, dest):
            downloaded += 1
        else:
            failed += 1

    print(f"Test assets: {downloaded} downloaded, {skipped} already present, {failed} unavailable.")


if __name__ == "__main__":
    main()
