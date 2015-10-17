function time_readable(dow, hours, minutes) {
	var dow_readable = {
		1 : "Monday",
		2 : "Tuesday",
		3 : "Wednesday",
		4 : "Thursday",
		5 : "Friday",
		6 : "Saturday",
		7 : "Sunday"
	}[dow];

	hours = ("0" + hours).slice(-2);
	minutes = ("0" + minutes).slice(-2);

	return dow_readable + ", " + hours + ":" + minutes;
}

function alarmlist_update() {
	$.get("waketimes_get", function (res) {
		$("#alarmlist").html("");
		JSON.parse(res).forEach(function (alarm) {
			$("#alarmlist").append($("<tr>")
				.append($("<td>").addClass("alarm_id").text(alarm.id))
				.append($("<td>").addClass("alarm_time").text(time_readable(alarm.dow, alarm.hrs, alarm.min)))
				.append($("<td>").append($("<a>").addClass("alarm_delete").data("alarmid", alarm.id).html("&#215;")))
			);
		});

		if (JSON.parse(res).length === 0) {
			$("#alarmlist").append($("<tr>").addClass("empty").append($("<th>").text("No alarms set")));
		}

		$(".alarm_delete").click(function () {
			$.get("waketime_del", { id : $(this).data("alarmid") }, function (res) {
				if (res !== "ok") alert("Error: " + res);
				alarmlist_update();
			});
		});
	});
}

$(function () {
	$('#intensitybuttons input[type="button"]').each(function (idx, button) {
		$(button).css("background-color", "rgba(0, 150, 255, " + $(button).data("color") / 100 + ")");
	});

	$('#intensitybuttons input[type="button"]').click(function () {
		$.get("intensity_set", { intensity : $(this).data("intensity") }, function (res) {
			if (res !== "ok") alert("Error: " + res);
		});
	});

	if ($("#alarmlist").length) alarmlist_update();

	$("#add").click(function () {
		var dow = $("#dow").val();
		var hours = parseInt($("#hours").val());
		var minutes = parseInt($("#minutes").val());

		if (isNaN(hours) || hours >= 24 || isNaN(minutes) || minutes >= 60) {
			alert("Invalid time!");
		} else {
			$.get("waketime_add", {	dow: dow, hrs : hours, min : minutes }, function (res) {
				if (res !== "ok") {
					alert("Error: " + res);
				} else {
					alarmlist_update();
				}
			});
		}
	});
});
