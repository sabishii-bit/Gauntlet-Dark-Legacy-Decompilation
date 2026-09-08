"""Fail-closed policy for the native-only reconstruction build.

This checks the build graph, not source authenticity or retail equivalence.
Derived compiler versions remain separately disclosed by build_provenance.
"""
import re

FORBIDDEN_RULES = {
    'frank', 'webfrank', 'webfrank_globalize_atree', 'globalize_atree',
    'p6frank', 'fix_exception_object', 'fix_exception_objects', 'retail_dol',
    'mwcc_extab', 'mwcc_sjis_extab',
}
NATIVE_PRODUCERS = {'mwcc', 'mwcc_sjis', 'as'}
REWRITER = re.compile(r'(?:\bextab\s+clean\b|\belf\s+fixup\b|'
                      r'(?:webfrank|p6frank|frank|retaildol|atree_exports|fix_exception_objects)\.py)')


def check_config(config, objects):
    if not getattr(config, 'native_only', False):
        return
    if config.object_postprocesses or config.custom_build_rules or config.custom_build_steps:
        raise ValueError('native-only builds forbid custom postprocessing rules/steps')
    for name, obj in objects.items():
        for option in ('frank_profile_mw_version', 'postprocess', 'extab_padding'):
            if obj.options.get(option) is not None:
                raise ValueError(f'native-only build forbids {option} on {name}')


def check_snapshot(snapshot):
    if snapshot.get('native_only') is not True:
        raise ValueError('native-only policy is not declared in the build snapshot')
    producers = {}
    for edge in snapshot['edges']:
        rule = edge['rule']
        command = snapshot['rules'].get(rule, {}).get('command_template', '')
        if rule in FORBIDDEN_RULES or REWRITER.search(command):
            raise ValueError(f'native-only build contains rewrite edge: {rule}')
        for output in edge['outputs']:
            if '/.postprocess/' in output.replace('\\', '/'):
                raise ValueError('native-only build has a postprocessed output: '+output)
            if output in producers:
                raise ValueError('duplicate build output: '+output)
            producers[output] = rule
    for unit in snapshot['units']:
        output = unit.get('source_object')
        if output and producers.get(output) not in NATIVE_PRODUCERS:
            raise ValueError('source object is not a direct compiler/assembler output: '+output)


if __name__ == '__main__':
    print(__doc__.strip())
