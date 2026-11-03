"""Original Research UI adapter for an already owned acceptance runner."""
import uuid

READS = {'Read', 'State', 'StaffState'}

def require(value, cause):
    if not value:
        raise RuntimeError(cause)

def catalogue_row(development, key):
    rows = [row for row in development['research']['rows'] if row['key'] == key]
    require(len(rows) == 1, 'Research key must identify one actual current catalogue row')
    return rows[0]

def operate(runner, key, command, label, peer='client-1'):
    """Return UI admission only. The caller must confirm the ordered receipt."""
    development = runner.single(label + '-fresh-development', peer, 'game_read_tutorial_development')
    require(development.get('mainthread_confirmed') is True and development.get('lifetime_confirmed') is True,
            'Research requires complete current native development readback')
    selected = catalogue_row(development, key)
    if command == 'Start':
        require(all(selected[field] is True for field in ('visible', 'can_choose_now', 'can_afford'))
                and selected['owned'] is False and selected['purchased'] is False
                and development['research']['project_present'] is False,
                'Original Research prerequisites, funds or current project reject Start')
    request = {'token': str(uuid.uuid4()), 'native_origin_uuid': development['snapshot_uuid'],
               'world_epoch': development['world_epoch'], 'world_generation': development['world_generation'],
               'operation': command, 'tech_key': key, 'expected_development': development}
    intent = {'key': key, 'operation': command, 'request': request, 'state': 'attempted'}
    runner.summary.setdefault('research_ui_intents', []).append(intent)
    runner.persist()
    result = runner.single(label + '-once', peer, 'game_native_research_ui', request)
    intent['result'] = result
    runner.persist()
    require(result.get('token') == request['token'] and result.get('unknown') is False
            and result.get('returned') is True and result.get('confirmed') is True,
            'Research UI return/token unconfirmed; inspect retained state and never replay')
    if command in READS:
        require(result.get('read_only') is True and result.get('attempted') is False,
                'Research read acknowledgement differs')
    else:
        require(result.get('attempted') is True, 'Research original UI call was not attempted')
    if command in ('Start', 'StaffOne', 'Commit'):
        require(result.get('queued') is True and result.get('route') == 'general_project'
                and result.get('submission_confirmed') is False
                and result.get('confirmation') == 'intent_queued',
                'Research UI did not retain its genuine ordered project intent')
        uuid.UUID(result['action_uuid'])
    intent['state'] = 'ui-admission-confirmed'
    runner.persist()
    return result
