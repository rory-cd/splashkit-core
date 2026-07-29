// Color Unit Tests

namespace SplashKit.CSharp.UnitTests;

/// <summary>
/// Tests color creation with RGB(A) values
/// </summary>
[Trait("Category", "Color")]
[Trait("Function", "rgba_color, rgb_color, red_of, blue_of, green_of, alpha_of")]
public class ColorCreation
{
    [Fact(DisplayName = "Can create RGB color with variable alpha values")]
    public void RGBAColor_SpecificAlpha_ReturnsCorrectValue()
    {
        Color c = RGBAColor(123, 45, 67, 200);
        Assert.Equal(123, RedOf(c));
        Assert.Equal(45, GreenOf(c));
        Assert.Equal(67, BlueOf(c));
        Assert.Equal(200, AlphaOf(c));
    }

    [Fact(DisplayName = "Can create RGB color with default alpha of 255 (opaque)")]
    public void RGBColor_NoAlpha_ReturnsOpaque()
    {
        Color c = RGBColor(10, 20, 30);
        Assert.Equal(10, RedOf(c));
        Assert.Equal(20, GreenOf(c));
        Assert.Equal(30, BlueOf(c));
        Assert.Equal(255, AlphaOf(c));
    }
}

/// <summary>
/// Tests converting a string to a color
/// </summary>
[Trait("Category", "Color")]
[Trait("Function", "string_to_color, red_of, blue_of, green_of, alpha_of")]
public class StringToColorConversion
{
    [Fact(DisplayName = "Valid color hex string returns expected color values")]
    public void StringToColor_ValidInput_ReturnsCorrectRGB()
    {
        Color red = StringToColor("#ff0000ff");
        Assert.Equal(255, RedOf(red));
        Assert.Equal(0, GreenOf(red));
        Assert.Equal(0, BlueOf(red));
        Assert.Equal(255, AlphaOf(red));
    }

    [Fact(DisplayName = "Invalid string returns fallback color of white")]
    public void StringToColor_InvalidInput_ReturnsWhite()
    {
        Color invalid = StringToColor("not_a_color");
        Assert.Equal(255, RedOf(invalid));
        Assert.Equal(255, GreenOf(invalid));
        Assert.Equal(255, BlueOf(invalid));
    }
}
