# Test XML Fixtures

## updateinfo_00.xml -- empty updates

<?xml version="1.0" encoding="UTF-8"?>
<updates>
</updates>

## updateinfo_01.xml -- comprehensive single update with all fields populated

<?xml version="1.0" encoding="UTF-8"?>
<updates>
  <update from="secresponseteam@foo.bar" status="final" type="enhancement" version="3">
    <id>foobarupdate_1</id>
    <title>title_1</title>
    <issued date="2012-12-12 00:00:00"/>
    <updated date="2012-12-12 00:00:00"/>
    <rights>rights_1</rights>
    <release>release_1</release>
    <pushcount>pushcount_1</pushcount>
    <severity>severity_1</severity>
    <summary>summary_1</summary>
    <description>description_1</description>
    <solution>solution_1</solution>
    <reboot_suggested>True</reboot_suggested>
    <references>
        <reference href="https://foobar/foobarupdate_1" id="1" type="self" title="update_1"/>
    </references>
    <pkglist>
      <collection short="foo.component">
        <name>Foo component</name>
        <package name="bar" version="2.0.1" release="3" epoch="0" arch="noarch" src="bar-2.0.1-3.src.rpm">
          <filename>bar-2.0.1-3.noarch.rpm</filename>
          <sum type="sha256">29be985e1f652cd0a29ceed6a1c49964d3618bddd22f0be3292421c8777d26c8</sum>
          <reboot_suggested/>
          <restart_suggested/>
          <relogin_suggested/>
        </package>
      </collection>
    </pkglist>
  </update>
</updates>

## updateinfo_02.xml.xz -- minimal update (all attributes/elements absent, tests NULL handling)

<?xml version="1.0" encoding="UTF-8"?>
<updates>
  <update>
    <references>
        <reference/>
    </references>
    <pkglist>
      <collection>
        <package>
        </package>
      </collection>
    </pkglist>
  </update>
</updates>

## updateinfo_03.xml -- 6 updates, including modular content and various edge cases

This is a larger fixture (130 lines) at testdata/updateinfo_files/updateinfo_03.xml containing 6 <update> records. Notable features:

Multiple collections per update (RHEA-2012:0059 has 2 collections, each with a <module> element)
* <module> elements with NSVCA (name, stream, version, context, arch)
* <reboot_suggested/> as empty element (vs with "True" content)
* Mangled date strings (e.g. "15mangled2") to test parsing tolerance
* Epoch timestamps as date values (e.g. "1555429284")
* Dates with " UTC" suffix (e.g. "2018-07-20 06:00:01 UTC")

The Python bindings attempt to parse dates via strptime with format "%Y-%m-%d %H:%M:%S", falling back to "%Y-%m-%d", then falling back to treating the whole string as an epoch integer. If none work, an error is raised. Test fixture updateinfo_03.xml includes deliberately mangled dates like "15mangled2", epoch-as-date "1555429284", and dates with " UTC" suffix "2018-07-29 06:00:01 UTC".

## updateinfo_ampersand.xml -- ampersand-in-values test

At testdata/modified_repo_files/updateinfo_ampersand.xml. Tests that &amp; is properly unescaped in both attribute values and text content throughout all fields.
