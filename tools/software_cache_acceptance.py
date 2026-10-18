"""Extend local acceptance with an observed native rating/fun replica gate."""
from __future__ import annotations
import sys
import time
from local_acceptance import AcceptanceRunner, AcceptanceError, parser, utc, log


def replica_reason(host: dict, client: dict, baseline: dict | None = None) -> str | None:
    for peer, engine in (("host", host), ("client", client)):
        if engine.get("in_game") is not True or engine.get("session_connected") is not True:
            return f"{peer} active world and connected session required"
        if engine.get("clock", {}).get("scale") != 0:
            return f"{peer} diagnostic calendar must be paused"
    state=client.get("software", {})
    source=host.get("software", {})
    if not state.get("replica_armed") or not state.get("replica_bound") or state.get("replica_error"):
        return f"client replica policy not bound: {state.get('replica_error')}"
    if state.get("applied_revision",0) < 1 or not state.get("bound_epoch"):
        return "client initial host revision not applied"
    if state.get("initial_rating") != 0 or state.get("initial_fun") != 0:
        return f"fresh client zero pair required; actual={state.get('initial_rating')}/{state.get('initial_fun')}"
    if (state.get("rating"),state.get("fun")) != (source.get("rating"),source.get("fun")):
        return "waiting for client cache that differs from current host"
    if state.get("rating") == 0 and state.get("fun") == 0:
        return "host pair did not cause an observable mutation"
    stream=client.get("software_sync",{})
    if stream.get("error"):
        return f"client software stream failed: {stream['error']}"
    if host.get("software_sync",{}).get("error"):
        return f"host software stream failed: {host['software_sync']['error']}"
    if baseline is None:
        return None
    for index, name in ((0,"rating"),(1,"fun")):
        current=state["calculations"][index]
        previous=baseline["calculations"][index]
        if current["forwarded"] != previous["forwarded"] or current["guard_rejected"] != previous["guard_rejected"]:
            return f"client {name} calculation forwarded or guard rejected after baseline"
        if current["suppressed"]-previous["suppressed"] < 2:
            return f"waiting for two suppressed client {name} cycles"
    tick=state["ticks"][1]
    old=baseline["ticks"][1]
    entered=tick["entered"]-old["entered"]
    if entered < 2 or tick["forwarded"]-old["forwarded"] != entered:
        return f"waiting for two forwarded client ticker cycles: entered={entered}"
    return None


class SoftwareAcceptance(AcceptanceRunner):
    def single(self,name,peer,tool,arguments=None):
        if peer=="host" and tool=="game_capture_snapshot":
            deadline=time.monotonic()+self.args.observe_seconds
            count=0
            while True:
                remaining=deadline-time.monotonic()
                if remaining<=0:
                    raise AcceptanceError("host software readiness timed out: natural nonzero cache required before snapshot; no snapshot attempted")
                count+=1
                receipt=self.job(f"software-host-source-{count:02d}","host",[{"tool":"game_inspect_engine","arguments":{}}],remaining=remaining)
                from local_acceptance import extract_result
                engine=extract_result(receipt,"game_inspect_engine")
                software=engine.get("software",{})
                if software.get("rating",0)!=0 or software.get("fun",0)!=0:
                    self.checked("host-natural-software-source",software)
                    break
                if count==1:
                    log("info","waiting for the host's native software calculation before snapshot")
        return super().single(name,peer,tool,arguments)

    def run(self):
        if super().run():
            return 1
        self.summary["passed"]=False
        self.summary["state"]="observing-software-cache"
        self.persist()
        try:
            host=self.inspect("software-host-baseline","host")
            client=self.inspect("software-client-baseline","client-1")
            reason=replica_reason(host,client)
            if reason and not reason.startswith("waiting for"):
                raise AcceptanceError(reason)
            self.checked("software-real-zero-to-host-pair",{"before":[0,0],"after":[client["software"]["rating"],client["software"]["fun"]],"epoch":client["software"]["bound_epoch"]})
            baseline=client["software"]
            deadline=time.monotonic()+self.args.observe_seconds
            count=0
            previous=None
            while True:
                remaining=deadline-time.monotonic()
                if remaining<=0:
                    raise AcceptanceError(f"software cycle observation timeout required={self.args.observe_seconds}s last_condition={previous}; no replay")
                count+=1
                host_receipt=self.job(f"software-host-cycle-{count:02d}","host",[{"tool":"game_inspect_engine","arguments":{}}],remaining=remaining)
                from local_acceptance import extract_result
                host=extract_result(host_receipt,"game_inspect_engine")
                remaining=deadline-time.monotonic()
                if remaining<=0:
                    raise AcceptanceError("software observation deadline after host read; no client mutation attempted")
                receipt=self.job(f"software-cycle-{count:02d}","client-1",[{"tool":"game_inspect_engine","arguments":{}}],remaining=remaining)
                from local_acceptance import extract_result
                client=extract_result(receipt,"game_inspect_engine")
                reason=replica_reason(host,client,baseline)
                if reason is None and client.get("software_sync",{}).get("applied_updates",0)<1:
                    reason="waiting for a real host cache change applied after initial snapshot"
                if reason is None:
                    break
                if not reason.startswith("waiting for"):
                    raise AcceptanceError(reason)
                if reason!=previous:
                    log("info",reason)
                    previous=reason
            self.checked("software-two-forwarded-ticker-cycles",{"host":host["software"],"client":client["software"],"poll_count":count})
            self.checked("software-running-native-update",{"host":host["software_sync"],"client":client["software_sync"]})
            self.capture("host")
            self.capture("client-1")
            self.summary["passed"]=True
            self.summary["state"]="confirmed-rating-fun-only"
        except Exception as error:
            self.summary["state"]="failed-software-cache"
            self.summary["error"]={"step":self.summary["step"],"cause":f"{type(error).__name__}: {error}","mutation_replayed":False}
            log("error",f"software acceptance failed: {error}")
        self.summary["finished_utc"]=utc()
        self.persist()
        log("info" if self.summary["passed"] else "error",f"software gate passed={self.summary['passed']} full_coop_proven=false summary={self.summary_path}")
        return 0 if self.summary["passed"] else 1


def main():
    argument_parser=parser()
    argument_parser.add_argument("--observe-seconds",type=float,default=55)
    args=argument_parser.parse_args()
    if args.cleanup:
        argument_parser.error("software observation requires retained instances; omit --cleanup")
    if not 1<=args.observe_seconds<=60:
        argument_parser.error("observe-seconds must be 1..60")
    if not 0.05<=args.step_timeout<=45 or not 0.05<=args.ready_timeout<=60:
        argument_parser.error("step-timeout must be 0.05..45 and ready-timeout 0.05..60")
    return SoftwareAcceptance(args).run()


if __name__=="__main__":
    sys.exit(main())
