using System.Text.Json;

public class Config
{
    [System.Text.Json.Serialization.JsonPropertyName("hotkeys")]
    public Dictionary<string, string> Hotkeys { get; set; } = new();

    public static Config Load(string path)
    {
        if (!File.Exists(path))
            return new Config();

        var json = File.ReadAllText(path);
        var opt = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };
        return JsonSerializer.Deserialize<Config>(json, opt) ?? new Config();
    }
}
