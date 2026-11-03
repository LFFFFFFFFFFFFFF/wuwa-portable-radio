using System;
using System.IO;
using System.Reflection;
using System.Text;
static class FixtureIdentity {
    static int Main(string[] args) {
        if(args.Length!=2)return 2;
        // This is only the handwritten fixture built from tests/fixture.cs.
        var assembly=Assembly.ReflectionOnlyLoadFrom(Path.GetFullPath(args[0]));
        var type=assembly.GetType("MotorcycleMusicPlayerController",true);
        var method=type.GetMethod("OnLeaveVehicle",BindingFlags.Public|BindingFlags.Instance);
        string header="#define EXPECTED_GUID \""+assembly.ManifestModule.ModuleVersionId+"\"\n#define EXPECTED_TOKEN 0x"+method.MetadataToken.ToString("x8")+"u\n";
        File.WriteAllText(args[1],header,Encoding.ASCII);return 0;
    }
}
