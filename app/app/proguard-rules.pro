# Note: reflection used by the app (HiddenApi.kt, driver/) targets framework classes such as
# dalvik.system.VMRuntime, which R8 never shrinks — no -keep rules are needed for them.
